cmake_minimum_required(VERSION 3.22)

project(PsytranceArpMatrix VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(PAM_BUILD_PLUGIN "Build the VST3/AU/Standalone plugin"          ON)
option(PAM_BUILD_TESTS  "Build the headless unit tests"                ON)
option(PAM_BUILD_TOOLS  "Build the factory preset builder"             ON)
option(PAM_AU_MIDI_FX   "Build the AU as a MIDI FX (Logic) instead of an instrument" OFF)
set(PAM_JUCE_PATH "" CACHE PATH "Use a local JUCE checkout instead of downloading it")

#==============================================================================
# JUCE
if(PAM_JUCE_PATH)
    add_subdirectory(${PAM_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

#==============================================================================
# Core engine: GUI-free, shared by plugin, tests and tools
add_library(pam_core INTERFACE)
target_sources(pam_core INTERFACE
    Source/Core/PatternTypes.cpp
    Source/Core/Scales.cpp
    Source/Core/StyleProfiles.cpp
    Source/Core/MidiAnalyzer.cpp
    Source/Core/InterlockMatrix.cpp
    Source/Core/PatternGenerator.cpp
    Source/Core/ChannelMatrix.cpp
    Source/Core/ArpEngine.cpp
    Source/IO/PatternSerializer.cpp
    Source/IO/PresetManager.cpp
    Source/IO/MultiMidiExporter.cpp
    Source/IO/MidiPatternImporter.cpp)
target_include_directories(pam_core INTERFACE Source)

set(PAM_HEADLESS_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    DONT_SET_USING_JUCE_NAMESPACE=1)

#==============================================================================
if(PAM_BUILD_PLUGIN)
    if(PAM_AU_MIDI_FX)
        set(PAM_IS_MIDI_EFFECT TRUE)
    else()
        set(PAM_IS_MIDI_EFFECT FALSE)
    endif()

    set(PAM_PLUGIN_FORMATS "" CACHE STRING "Override plugin formats (e.g. \"VST3\"); empty = VST3;Standalone(;AU on macOS)")
    if(PAM_PLUGIN_FORMATS)
        set(PAM_FORMATS ${PAM_PLUGIN_FORMATS})
    else()
        set(PAM_FORMATS VST3 Standalone)
        if(APPLE)
            list(APPEND PAM_FORMATS AU)
        endif()
    endif()

    juce_add_plugin(PsytranceArpMatrix
        COMPANY_NAME                "PsyArpLab"
        COMPANY_WEBSITE             "https://github.com/"
        PLUGIN_MANUFACTURER_CODE    Psyl
        PLUGIN_CODE                 Pam4
        FORMATS                     ${PAM_FORMATS}
        PRODUCT_NAME                "Psytrance Arp Matrix"
        IS_SYNTH                    TRUE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           TRUE
        IS_MIDI_EFFECT              ${PAM_IS_MIDI_EFFECT}
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             Instrument Generator
        AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

    juce_generate_juce_header(PsytranceArpMatrix)

    # Factory presets are embedded and installed to Documents/PsytranceArpMatrix on first run
    file(GLOB_RECURSE PAM_PRESET_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/PsytranceArpMatrix/Presets/*.json)
    juce_add_binary_data(PamFactoryPresets NAMESPACE PamPresets SOURCES ${PAM_PRESET_FILES})

    target_sources(PsytranceArpMatrix PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/FactoryPresets.cpp
        Source/UI/StepGridComponent.cpp
        Source/UI/PianoRollComponent.cpp
        Source/UI/ChannelStripComponent.cpp
        Source/UI/MidiDragSource.cpp)

    target_compile_definitions(PsytranceArpMatrix PUBLIC
        ${PAM_HEADLESS_DEFS}
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(PsytranceArpMatrix
        PRIVATE
            pam_core
            PamFactoryPresets
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

#==============================================================================
if(PAM_BUILD_TESTS)
    juce_add_console_app(PamTests PRODUCT_NAME "PamTests")
    target_sources(PamTests PRIVATE Tests/CoreTests.cpp)
    target_compile_definitions(PamTests PRIVATE ${PAM_HEADLESS_DEFS} JUCE_UNIT_TESTS=1)
    target_link_libraries(PamTests PRIVATE
        pam_core juce::juce_core juce::juce_audio_basics juce::juce_events
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME PamTests COMMAND PamTests)
endif()

if(PAM_BUILD_TOOLS)
    juce_add_console_app(PamPresetBuilder PRODUCT_NAME "PamPresetBuilder")
    target_sources(PamPresetBuilder PRIVATE tools/PresetBuilder.cpp)
    target_compile_definitions(PamPresetBuilder PRIVATE ${PAM_HEADLESS_DEFS})
    target_link_libraries(PamPresetBuilder PRIVATE
        pam_core juce::juce_core juce::juce_audio_basics juce::juce_events
        juce::juce_recommended_config_flags)
endif()
