cmake_minimum_required(VERSION 3.22)

if(CMAKE_BINARY_DIR MATCHES "'")
    message(FATAL_ERROR
        "CMAKE_BINARY_DIR contains an apostrophe ('), which breaks JUCE's "
        "POST_BUILD shell steps (CMake does not escape apostrophes in the "
        "shell command chains it emits). Configure the build directory "
        "somewhere without an apostrophe, e.g.:\n"
        "  cmake -S . -B /tmp/aerodynamics-build -G Ninja")
endif()

project(AeroDynamicsPro VERSION 0.6.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# PRODUCT_NAME intentionally has no apostrophe: JUCE's generated VST3 POST_BUILD
# steps do not escape apostrophes in the shell command chains they emit, and
# PRODUCT_NAME feeds directly into the .vst3 bundle path. "Gabci's AeroDynamics"
# is used as the on-screen display name instead (editor wordmark / About panel),
# the same bundle-name vs. on-screen-name split already proven in the sibling
# projects (SpectralCarve Pro, MorphicPhaser).
juce_add_plugin(AeroDynamicsProPlugin
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Gabci Audio"
    BUNDLE_ID "com.gabciaudio.aerodynamicspro"
    PLUGIN_MANUFACTURER_CODE Gabc
    PLUGIN_CODE Arod
    FORMATS VST3 Standalone
    PRODUCT_NAME "AeroDynamics Pro"
    COMPANY_COPYRIGHT "Gabci Audio (c) 2026"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_compile_definitions(AeroDynamicsProPlugin
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_sources(AeroDynamicsProPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PluginParameters.cpp
        Source/OversamplingManager.cpp
        Source/FluidEngine.cpp
        Source/DcBlocker.cpp
        Source/AeroDynamicsLookAndFeel.cpp
        Source/FluidVisualizerComponent.cpp
        Source/AboutPanel.cpp
)

target_link_libraries(AeroDynamicsProPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(AERODYNAMICSPRO_BUILD_TESTS "Build the AeroDynamics Pro Catch2 test suite" ON)
if(AERODYNAMICSPRO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
