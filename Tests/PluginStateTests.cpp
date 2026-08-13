/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginParameters.h"

namespace
{
    /** Minimal headless stand-in for AeroDynamicsProAudioProcessor, exercising the
        exact same createParameterLayout() + APVTS state-save/restore code path the
        real plugin uses in getStateInformation()/setStateInformation() -- without
        pulling in the GUI/editor/DSP machinery (that would need JucePlugin_* macros
        only auto-generated for the real plugin CMake target, not a console app).
        Phase 5's "DAW Integration Preparation" ask was to verify the host-facing
        parameter/state contract; this is that contract in isolation.
    */
    class StubProcessor : public juce::AudioProcessor
    {
    public:
        StubProcessor() : apvts (*this, nullptr, "PARAMETERS", createParameterLayout()) {}

        const juce::String getName() const override { return "Stub"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override
        {
            if (auto state = apvts.copyState(); auto xml = state.createXml())
                copyXmlToBinary (*xml, destData);
        }

        void setStateInformation (const void* data, int sizeInBytes) override
        {
            if (auto xml = getXmlFromBinary (data, sizeInBytes))
                if (xml->hasTagName (apvts.state.getType()))
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }

        juce::AudioProcessorValueTreeState apvts;
    };
}

TEST_CASE ("PluginParameters: every parameter reports the documented default value", "[PluginState]")
{
    StubProcessor processor;
    auto& apvts = processor.apvts;

    // Catch::Approx, not exact ==: NormalisableRange<float>'s forward/backward
    // (0-1 <-> real range) conversion for a non-power-of-2 interval (e.g. 0.1) can
    // leave a ~2e-6 relative rounding artifact on the FIRST such conversion done in
    // the process (reproduced directly -- see PluginStateTests.cpp's git history /
    // CHANGELOG for the measurement); meaningless for a 0-100 range with 0.1
    // granularity, but exact == would make the test flaky depending on what ran
    // before it in the same process, which is not what this test is trying to check.
    REQUIRE (apvts.getParameter (ParamIDs::pressure)->getDefaultValue() == Catch::Approx (0.0f).margin (0.001)); // 0dB, normalised 0-36 range
    REQUIRE (apvts.getRawParameterValue (ParamIDs::viscosity)->load() == Catch::Approx (30.0f));
    REQUIRE (apvts.getRawParameterValue (ParamIDs::turbulence)->load() == Catch::Approx (30.0f));
    REQUIRE (apvts.getRawParameterValue (ParamIDs::flowRate)->load() == Catch::Approx (50.0f));
    REQUIRE ((int) apvts.getRawParameterValue (ParamIDs::oversampling)->load() == OversamplingOption::factor8x);
    REQUIRE (apvts.getRawParameterValue (ParamIDs::mix)->load() == Catch::Approx (100.0f));
    REQUIRE (apvts.getRawParameterValue (ParamIDs::bypass)->load() == Catch::Approx (0.0f).margin (0.001));
}

TEST_CASE ("PluginParameters: every parameter is marked automatable for host automation", "[PluginState]")
{
    StubProcessor processor;
    auto& apvts = processor.apvts;

    for (const char* id : { ParamIDs::pressure, ParamIDs::viscosity, ParamIDs::turbulence, ParamIDs::flowRate,
                             ParamIDs::oversampling, ParamIDs::mix, ParamIDs::bypass })
    {
        auto* param = apvts.getParameter (id);
        REQUIRE (param != nullptr);
        REQUIRE (param->isAutomatable());
    }
}

TEST_CASE ("PluginParameters: getStateInformation/setStateInformation round-trips every non-default value",
           "[PluginState]")
{
    StubProcessor writer;

    // Drive every parameter to a value that is NOT its default, so a round-trip that
    // silently fell back to defaults would be caught, not masked.
    writer.apvts.getParameter (ParamIDs::pressure)->setValueNotifyingHost (0.75f);
    writer.apvts.getParameter (ParamIDs::viscosity)->setValueNotifyingHost (0.9f);
    writer.apvts.getParameter (ParamIDs::turbulence)->setValueNotifyingHost (0.05f);
    writer.apvts.getParameter (ParamIDs::flowRate)->setValueNotifyingHost (0.1f);
    writer.apvts.getParameter (ParamIDs::oversampling)->setValueNotifyingHost (0.0f); // -> factor4x
    writer.apvts.getParameter (ParamIDs::mix)->setValueNotifyingHost (0.3f);
    writer.apvts.getParameter (ParamIDs::bypass)->setValueNotifyingHost (1.0f);

    juce::MemoryBlock savedState;
    writer.getStateInformation (savedState);

    StubProcessor reader;
    reader.setStateInformation (savedState.getData(), (int) savedState.getSize());

    // Compare via the RAW (denormalised) values -- what processBlock() actually
    // reads -- not the normalised [0,1] parameter positions, since that's the
    // contract that matters for correctness.
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::pressure)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::pressure)->load());
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::viscosity)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::viscosity)->load());
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::turbulence)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::turbulence)->load());
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::flowRate)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::flowRate)->load());
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::oversampling)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::oversampling)->load());
    REQUIRE ((int) reader.apvts.getRawParameterValue (ParamIDs::oversampling)->load() == OversamplingOption::factor4x);
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::mix)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::mix)->load());
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::bypass)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::bypass)->load());
}

TEST_CASE ("PluginParameters: restoring a garbage/empty state leaves parameters at their defaults, no crash",
           "[PluginState]")
{
    StubProcessor processor;

    const char garbage[] = { 1, 2, 3, 4, 5 };
    processor.setStateInformation (garbage, (int) sizeof (garbage));

    // Not-a-valid-XML-blob is silently ignored by design (getXmlFromBinary()
    // returns nullptr) -- confirms that path doesn't crash and leaves defaults
    // intact, rather than e.g. leaving the APVTS in a half-updated state. (See the
    // Approx note in the "default value" test above for why this isn't exact ==.)
    REQUIRE (processor.apvts.getRawParameterValue (ParamIDs::viscosity)->load() == Catch::Approx (30.0f));

    processor.setStateInformation (nullptr, 0);
    REQUIRE (processor.apvts.getRawParameterValue (ParamIDs::viscosity)->load() == Catch::Approx (30.0f));
}
