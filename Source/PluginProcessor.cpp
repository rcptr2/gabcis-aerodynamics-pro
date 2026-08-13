/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

//==============================================================================
AeroDynamicsProAudioProcessor::AeroDynamicsProAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pressureParam = apvts.getRawParameterValue (ParamIDs::pressure);
    viscosityParam = apvts.getRawParameterValue (ParamIDs::viscosity);
    turbulenceParam = apvts.getRawParameterValue (ParamIDs::turbulence);
    flowRateParam = apvts.getRawParameterValue (ParamIDs::flowRate);
    oversamplingParam = apvts.getRawParameterValue (ParamIDs::oversampling);
    mixParam = apvts.getRawParameterValue (ParamIDs::mix);
    bypassParam = apvts.getRawParameterValue (ParamIDs::bypass);
}

AeroDynamicsProAudioProcessor::~AeroDynamicsProAudioProcessor() = default;

//==============================================================================
bool AeroDynamicsProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
           && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void AeroDynamicsProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling.prepare (2, samplesPerBlock);

    for (auto& engine : fluidEngines)
        engine.reset();

    for (auto& blocker : dcBlockers)
        blocker.prepare (sampleRate);

    dryBuffer.setSize (2, samplesPerBlock, false, false, true);

    const auto qIndex = (int) oversamplingParam->load();
    oversampling.setQuality ((OversamplingManager::Quality) qIndex);
    lastReportedOversamplingIndex = qIndex;

    dryLatencyCompensation.prepare (juce::dsp::ProcessSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 });
    dryLatencyCompensation.setDelay ((float) oversampling.getLatencySamples());

    // Flow Rate's delay line (v0.6.0): sized for the WORST-CASE oversampling factor
    // (8x) regardless of which quality is currently selected, so a later quality
    // switch never needs to reallocate the buffer on the audio thread -- only
    // dryLatencyCompensation's fixed 128-sample bound could assume a host-rate
    // -independent size; this one depends on the actual host sample rate, hence
    // computed here rather than as a compile-time constant.
    currentOversampledRate = sampleRate * (double) oversampling.getFactor();
    const auto maxFlowRateDelaySamples = (int) std::ceil (sampleRate * 8.0 * (double) kMaxFlowRateDelayMs / 1000.0) + 4;
    for (auto& delayLine : flowRateDelayLines)
    {
        delayLine.setMaximumDelayInSamples (maxFlowRateDelaySamples);
        delayLine.prepare (juce::dsp::ProcessSpec { currentOversampledRate, (juce::uint32) samplesPerBlock, 1 });
        delayLine.reset();
    }

    const auto effectiveMix = bypassParam->load() > 0.5f ? 0.0f : mixParam->load() / 100.0f;
    mixSmoothed.reset (sampleRate, 0.02); // 20ms crossfade, avoids a click on Mix/Bypass changes
    mixSmoothed.setCurrentAndTargetValue (effectiveMix);

    // Ramp time is defined in oversampled samples (FluidEngine::setParameters() is
    // called once per oversampled sample, see processBlock()); a later oversampling
    // quality change makes this ramp's real-world duration slightly off (e.g. an 8x
    // ramp continuing at a rate computed for 4x), a cosmetic-only imprecision not
    // worth resetting the smoother's in-flight state over.
    flowRateSmoothed.reset (sampleRate * oversampling.getFactor(), 0.02);
    flowRateSmoothed.setCurrentAndTargetValue (flowRateParam->load() / 100.0f);

    // Safe directly here (not via notifyLatencyChanged()/AsyncUpdater): prepareToPlay()
    // always runs on the message thread, never the audio thread.
    setLatencySamples ((int) std::ceil (oversampling.getLatencySamples()));
}

void AeroDynamicsProAudioProcessor::releaseResources()
{
}

void AeroDynamicsProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Detect an Oversampling-quality change (host automation or UI). Switching is
    // itself real-time safe (OversamplingManager just swaps which pre-built chain is
    // active), but the resulting latency change must be reported to the host from the
    // message thread -- notifyLatencyChanged() just triggers the AsyncUpdater here.
    // The dry-path delay line is re-pointed at the new latency value immediately
    // (not smoothed -- a quality change is a rare, deliberate user/host action, not
    // audio-rate automation, so a brief transient there is an acceptable trade-off
    // against the complexity of smoothing a delay line's own delay time).
    const auto qIndex = (int) oversamplingParam->load();
    if (qIndex != lastReportedOversamplingIndex)
    {
        oversampling.setQuality ((OversamplingManager::Quality) qIndex);
        lastReportedOversamplingIndex = qIndex;
        dryLatencyCompensation.setDelay ((float) oversampling.getLatencySamples());
        currentOversampledRate = (double) getSampleRate() * (double) oversampling.getFactor();
        notifyLatencyChanged();
    }

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::AudioBlock<const float> inputBlock (block);

    auto oversampledBlock = oversampling.processSamplesUp (inputBlock);
    const auto numOSChannels = juce::jmin ((int) oversampledBlock.getNumChannels(), (int) fluidEngines.size());
    const auto numOSSamples = oversampledBlock.getNumSamples();

    const auto viscosity01 = viscosityParam->load() / 100.0f;
    const auto turbulence01 = turbulenceParam->load() / 100.0f;
    flowRateSmoothed.setTargetValue (flowRateParam->load() / 100.0f);

    for (auto& engine : fluidEngines)
        engine.setParameters (viscosity01, turbulence01);

    const auto pressureGain = juce::Decibels::decibelsToGain (pressureParam->load());
    const auto maxFlowRateDelaySamples = (float) (currentOversampledRate * (double) kMaxFlowRateDelayMs / 1000.0);

    std::array<float*, 2> channelData { nullptr, nullptr };
    for (int ch = 0; ch < numOSChannels; ++ch)
        channelData[(size_t) ch] = oversampledBlock.getChannelPointer ((size_t) ch);

    // Sample-major (not channel-major): Flow Rate must advance exactly once per
    // oversampled sample, shared identically across channels, not once per
    // (channel, sample) pair.
    for (size_t i = 0; i < numOSSamples; ++i)
    {
        const auto smoothedFlowRate01 = flowRateSmoothed.getNextValue();
        const auto delaySamples = smoothedFlowRate01 * maxFlowRateDelaySamples;

        for (int ch = 0; ch < numOSChannels; ++ch)
        {
            auto& engine = fluidEngines[(size_t) ch];
            const auto shaped = engine.processSample (channelData[(size_t) ch][i] * pressureGain);

            auto& delayLine = flowRateDelayLines[(size_t) ch];
            delayLine.setDelay (delaySamples);
            delayLine.pushSample (0, shaped);
            channelData[(size_t) ch][i] = delayLine.popSample (0);
        }
    }

    // Publish this block's final grid state for the Phase 4 visualizer -- cheap
    // (two small float array copies), once per block, never touches audio-thread
    // state from the UI side (see VisualizationPublisher.h for the lock-free
    // hand-off).
    {
        VisualizationPublisher::Snapshot snapshot;
        snapshot.gridL = fluidEngines[0].getGrid();
        snapshot.gridR = fluidEngines[1].getGrid();
        visualizationPublisher.publish (snapshot);
    }

    oversampling.processSamplesDown (block);

    // DC blocker: the wet signal only, post-downsample (cheaper than pre-oversample,
    // and correctness doesn't need the oversampled rate -- a 10Hz one-pole filter's
    // behaviour is essentially rate-independent relative to its own cutoff).
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        auto& blocker = dcBlockers[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
            wet[i] = blocker.processSample (wet[i]);
    }

    // Delay the saved dry copy by the oversampling chain's latency so it lines up in
    // time with the (inherently latent) wet signal before mixing -- see the
    // class-level comment in PluginProcessor.h for why this is required.
    juce::dsp::AudioBlock<float> dryBlock (dryBuffer);
    dryBlock = dryBlock.getSubBlock (0, (size_t) numSamples);
    dryLatencyCompensation.process (juce::dsp::ProcessContextReplacing<float> (dryBlock));

    const auto effectiveMix = bypassParam->load() > 0.5f ? 0.0f : mixParam->load() / 100.0f;
    mixSmoothed.setTargetValue (effectiveMix);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        auto* dry = dryBuffer.getReadPointer (ch);
        auto mixState = mixSmoothed;

        for (int i = 0; i < numSamples; ++i)
        {
            // Linear crossfade, not equal-power: v0.5.0 used an equal-power
            // (sin/cos) curve, but live testing showed it made the knob's top half
            // feel dead -- at 50% the wet gain is already sin(pi/4) ~= 0.707, and
            // sin() is flattening out that close to its peak, so 50->100% barely
            // changed the sound. A plain linear law matches what a Dry/Wet knob's
            // position is expected to mean (physically proportional dry/wet
            // balance), at the cost of a small, normal-for-this-kind-of-control
            // loudness dip near the middle -- an accepted, well-understood
            // trade-off for a Mix knob specifically (unlike a stereo pan control,
            // where equal-power is the right call).
            const auto mix = mixState.getNextValue();
            wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
        }
    }

    mixSmoothed.skip (numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* AeroDynamicsProAudioProcessor::createEditor()
{
    return new AeroDynamicsProAudioProcessorEditor (*this);
}

bool AeroDynamicsProAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String AeroDynamicsProAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AeroDynamicsProAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AeroDynamicsProAudioProcessor::producesMidi() const
{
    return false;
}

bool AeroDynamicsProAudioProcessor::isMidiEffect() const
{
    return false;
}

double AeroDynamicsProAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
int AeroDynamicsProAudioProcessor::getNumPrograms()
{
    return 1;
}

int AeroDynamicsProAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AeroDynamicsProAudioProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String AeroDynamicsProAudioProcessor::getProgramName (int /*index*/)
{
    return {};
}

void AeroDynamicsProAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

//==============================================================================
void AeroDynamicsProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AeroDynamicsProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
void AeroDynamicsProAudioProcessor::notifyLatencyChanged()
{
    triggerAsyncUpdate();
}

void AeroDynamicsProAudioProcessor::handleAsyncUpdate()
{
    // Runs on the message thread (guaranteed by juce::AsyncUpdater), so this is
    // always safe even though the triggering quality-change detection happens on the
    // audio thread in processBlock().
    setLatencySamples ((int) std::ceil (oversampling.getLatencySamples()));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AeroDynamicsProAudioProcessor();
}
