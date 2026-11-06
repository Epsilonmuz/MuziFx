cmake_minimum_required(VERSION 3.22)
project(MuziFX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Alternative to the .jucer projects. JUCE: point at a full JUCE checkout with -DJUCE_DIR=/path/to/JUCE,
# otherwise JUCE 9.0.5 is downloaded.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.5 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(MZ_FORMATS AU VST3 Standalone)
else()
    set(MZ_FORMATS VST3 Standalone)
endif()
if(DEFINED MZ_FORMATS_OVERRIDE)
    set(MZ_FORMATS ${MZ_FORMATS_OVERRIDE})
endif()

set(MZ_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

# ---------------------------------------------------------------- Muzi EQ
juce_add_plugin(MuziEQ
    COMPANY_NAME "Muzi"
    PRODUCT_NAME "Muzi EQ"
    VERSION 0.1.0
    PLUGIN_MANUFACTURER_CODE Muzi
    PLUGIN_CODE MzEq
    FORMATS ${MZ_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)
target_sources(MuziEQ PRIVATE Source/eq/EqProcessor.cpp Source/eq/EqEditor.cpp)
target_include_directories(MuziEQ PRIVATE Source/eq Source/shared)
target_compile_definitions(MuziEQ PUBLIC ${MZ_DEFS})
target_link_libraries(MuziEQ PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- Muzi Glue
juce_add_plugin(MuziGlue
    COMPANY_NAME "Muzi"
    PRODUCT_NAME "Muzi Glue"
    VERSION 0.1.0
    PLUGIN_MANUFACTURER_CODE Muzi
    PLUGIN_CODE MzGl
    FORMATS ${MZ_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)
target_sources(MuziGlue PRIVATE Source/glue/GlueProcessor.cpp Source/glue/GlueEditor.cpp)
target_include_directories(MuziGlue PRIVATE Source/glue Source/shared)
target_compile_definitions(MuziGlue PUBLIC ${MZ_DEFS})
target_link_libraries(MuziGlue PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- test tools (not part of the plugins)
#   cmake -B build -DMZ_BUILD_TESTS=ON
option(MZ_BUILD_TESTS "Build the headless VST3 host and GUI snapshot tools" OFF)
if(MZ_BUILD_TESTS)
    juce_add_console_app(MuziHostTest PRODUCT_NAME "MuziHostTest")
    target_sources(MuziHostTest PRIVATE tests/host/HostTest.cpp)
    target_compile_definitions(MuziHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 ${MZ_DEFS})
    target_link_libraries(MuziHostTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

    foreach(P EQ Glue)
        string(TOLOWER ${P} p)
        juce_add_console_app(MuziSnapshot${P} PRODUCT_NAME "MuziSnapshot${P}")
        if(P STREQUAL "EQ")
            target_sources(MuziSnapshotEQ PRIVATE tests/snapshot/SnapshotEq.cpp Source/eq/EqProcessor.cpp Source/eq/EqEditor.cpp)
        else()
            target_sources(MuziSnapshotGlue PRIVATE tests/snapshot/SnapshotGlue.cpp Source/glue/GlueProcessor.cpp Source/glue/GlueEditor.cpp)
        endif()
        target_include_directories(MuziSnapshot${P} PRIVATE Source/${p} Source/shared)
        target_compile_definitions(MuziSnapshot${P} PRIVATE ${MZ_DEFS} JucePlugin_Name="Muzi" JucePlugin_Build_Standalone=0)
        target_link_libraries(MuziSnapshot${P} PRIVATE juce::juce_dsp juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics
            PUBLIC juce::juce_recommended_config_flags)
    endforeach()
endif()
