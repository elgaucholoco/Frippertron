cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) et cible macOS minimale
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Frippertron VERSION 2.0.0)   # à incrémenter à chaque version, voir CHANGELOG.md

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE : soit le dossier JUCE/ à côté de ce fichier, soit téléchargé automatiquement
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Frippertron
    COMPANY_NAME "Carlos"
    BUNDLE_ID "ch.carlos.frippertron"
    PLUGIN_MANUFACTURER_CODE Crls
    PLUGIN_CODE Frpt
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Frippertron"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE        # copie dans ~/Library/Audio/Plug-Ins/
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Frippertron PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Frippertron PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Frippertron
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# Tests hors ligne du traitement audio : cmake -B build -DFRIPPERTRON_TESTS=ON
option(FRIPPERTRON_TESTS "Compiler les tests hors ligne" OFF)
if(FRIPPERTRON_TESTS)
    juce_add_console_app(FrippertronTests PRODUCT_NAME "FrippertronTests")
    target_sources(FrippertronTests PRIVATE
        Tests/ProcessorTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(FrippertronTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(FrippertronTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
