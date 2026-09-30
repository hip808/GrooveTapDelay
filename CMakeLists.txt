cmake_minimum_required(VERSION 3.22)

project(GrooveTapDelay VERSION 1.1.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

set(JUCE_PATH "$ENV{HOME}/dev/JUCE" CACHE PATH "Path to a JUCE checkout")
option(MOKU_COPY_PLUGIN "Install the plugin after building" OFF)
add_subdirectory("${JUCE_PATH}" JUCE_build)

juce_add_plugin(GrooveTapDelay
    COMPANY_NAME "AlanIPAudio"
    PLUGIN_MANUFACTURER_CODE Alnp
    PLUGIN_CODE Gtd1
    FORMATS VST3
    PRODUCT_NAME "MOKU AI UD Stomp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${MOKU_COPY_PLUGIN}
    VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")

juce_add_binary_data(StompAssets SOURCES Assets/faceplate.png Assets/controls.png)

target_sources(GrooveTapDelay
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/FactoryPresets.cpp)

target_compile_definitions(GrooveTapDelay
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GrooveTapDelay
    PRIVATE
        StompAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

include(CTest)
if(BUILD_TESTING)
    add_executable(MokuTests)
    target_sources(MokuTests PRIVATE Tests/RegressionTests.cpp)
    target_link_libraries(MokuTests PRIVATE GrooveTapDelay StompAssets juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(MokuTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    add_test(NAME MokuRegression COMMAND MokuTests)
endif()
