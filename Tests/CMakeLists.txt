include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(AeroDynamicsProTests
    PRODUCT_NAME "AeroDynamicsProTests"
)

target_sources(AeroDynamicsProTests
    PRIVATE
        FluidEngineTests.cpp
        OversamplingManagerTests.cpp
        DcBlockerTests.cpp
        VisualizationPublisherTests.cpp
        PluginStateTests.cpp
        PluginProcessorTests.cpp
        PluginEditorTests.cpp
        ${CMAKE_SOURCE_DIR}/Source/FluidEngine.cpp
        ${CMAKE_SOURCE_DIR}/Source/OversamplingManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/DcBlocker.cpp
        ${CMAKE_SOURCE_DIR}/Source/PluginParameters.cpp
        ${CMAKE_SOURCE_DIR}/Source/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/Source/PluginEditor.cpp
        ${CMAKE_SOURCE_DIR}/Source/AeroDynamicsLookAndFeel.cpp
        ${CMAKE_SOURCE_DIR}/Source/FluidVisualizerComponent.cpp
        ${CMAKE_SOURCE_DIR}/Source/AboutPanel.cpp
)

target_include_directories(AeroDynamicsProTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

# PluginProcessor.cpp/AboutPanel.cpp reference JucePlugin_Name/JucePlugin_VersionString,
# which juce_add_plugin() auto-generates for the real plugin target but a plain
# juce_add_console_app() target does not -- defined manually here (matching the main
# CMakeLists' PRODUCT_NAME / PROJECT_VERSION) so PluginProcessorTests.cpp can link and
# exercise the REAL AeroDynamicsProAudioProcessor, not a stand-in, for its
# mid-stream-switching edge case tests.
target_compile_definitions(AeroDynamicsProTests
    PRIVATE
        JucePlugin_Name="AeroDynamics Pro"
        JucePlugin_VersionString="${PROJECT_VERSION}"
)

target_link_libraries(AeroDynamicsProTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
