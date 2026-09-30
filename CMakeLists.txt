cmake_minimum_required(VERSION 3.22)

project(MokuAIUDStomp VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

add_subdirectory($ENV{HOME}/dev/JUCE JUCE_build)

juce_add_binary_data(MokuAIUDStompAssets
    SOURCES
        Resources/PanelBackground.png
        Resources/Knob.png
        Resources/Button.png)

juce_add_plugin(MokuAIUDStomp
    COMPANY_NAME "MokuAI"
    PLUGIN_MANUFACTURER_CODE Mkai
    PLUGIN_CODE Uds1
    FORMATS VST3
    PRODUCT_NAME "MOKU AI 8Tap"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")

target_sources(MokuAIUDStomp
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/FactoryPresets.cpp)

target_compile_definitions(MokuAIUDStomp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MokuAIUDStomp
    PRIVATE
        MokuAIUDStompAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
