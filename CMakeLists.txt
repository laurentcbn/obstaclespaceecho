cmake_minimum_required(VERSION 3.22)
project(SpaceEcho VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# ─── Fetch JUCE ───────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ─── Plugin target ────────────────────────────────────────────────────────────
juce_add_plugin(SpaceEcho
    COMPANY_NAME                "Obstacle"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    Obst
    PLUGIN_CODE                 OSpE
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Obstacle Space Echo"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx" "Delay"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_generate_juce_header(SpaceEcho)

target_sources(SpaceEcho PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(SpaceEcho PRIVATE Source)

target_compile_definitions(SpaceEcho PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SpaceEcho
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
