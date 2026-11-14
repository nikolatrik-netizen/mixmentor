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
juce_add_binary_data(MixMentorData SOURCES knowledge.json Inter-Light.ttf Inter-Regular.ttf Inter-SemiBold.ttf)

target_sources(MixMentor PRIVATE
    KnowledgeBase.cpp
    PluginProcessor.cpp
    PluginEditor.cpp
    SpectrumView.cpp
    LevelMeter.cpp
    Theme.cpp
    MelodyEngine.cpp
    MelodyCore.cpp
    MelodyPanel.cpp)

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

#==============================================================================
# Mix Mentor Melody: a second plugin (instrument) that turns your melody into new ideas.
juce_add_plugin(MixMentorMelody
    COMPANY_NAME                "MixMentor"
    PLUGIN_MANUFACTURER_CODE    Mxmt
    PLUGIN_CODE                 Mxm2
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Mix Mentor Melody"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Generator
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    # Copy the result next to Mix Mentor so the GitHub build uploads both plugins together.
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_COPY_DIR               "${CMAKE_BINARY_DIR}/MixMentor_artefacts/Release/VST3"
    AU_COPY_DIR                 "${CMAKE_BINARY_DIR}/MixMentor_artefacts/Release/AU")

target_sources(MixMentorMelody PRIVATE
    KnowledgeBase.cpp
    Theme.cpp
    MelodyEngine.cpp
    MelodyCore.cpp
    MelodyPanel.cpp
    MelodyProcessor.cpp)

target_compile_definitions(MixMentorMelody PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MixMentorMelody
    PRIVATE
        MixMentorData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Building Mix Mentor (what the GitHub workflow asks for) also builds Mix Mentor Melody.
add_dependencies(MixMentor_VST3 MixMentorMelody_VST3)
if(APPLE)
    add_dependencies(MixMentor_AU MixMentorMelody_AU)
endif()
