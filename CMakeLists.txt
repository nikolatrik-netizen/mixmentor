cmake_minimum_required(VERSION 3.22)
project(MixMentor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically the first time you configure.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MixMentor
    COMPANY_NAME                "MixMentor"
    PLUGIN_MANUFACTURER_CODE    Mxmt
    PLUGIN_CODE                 Mxm1
    FORMATS                     VST3 AU Standalone   # AU is only built on macOS
    PRODUCT_NAME                "Mix Mentor"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Analyzer
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

# The genre knowledge base is compiled into the plugin.
juce_add_binary_data(MixMentorData SOURCES Resources/knowledge.json)

target_sources(MixMentor PRIVATE
    Source/KnowledgeBase.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpectrumView.cpp
    Source/LevelMeter.cpp)

target_compile_definitions(MixMentor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MixMentor
    PRIVATE
        MixMentorData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
