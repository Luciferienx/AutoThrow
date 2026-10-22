cmake_minimum_required(VERSION 3.22)
project(AutoThrow VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sous Windows : runtime statique -> le .vst3 marche sans installer les "Visual C++ Redistributable"
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE est téléchargé automatiquement au premier build
# (ou utilise un dossier local : cmake -B build -DJUCE_DIR=C:/chemin/vers/JUCE)
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(AutoThrow
    COMPANY_NAME                "Lucif"
    PLUGIN_MANUFACTURER_CODE    Lcif
    PLUGIN_CODE                 Athr
    PRODUCT_NAME                "AutoThrow"
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx Reverb
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(AutoThrow PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AutoThrow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AutoThrow
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Test hors-DAW du moteur DSP (optionnel) : cmake -B build -DAUTOTHROW_TESTS=ON
option(AUTOTHROW_TESTS "Build the offline DSP test" OFF)
if(AUTOTHROW_TESTS)
    add_executable(autothrow_test test/render_test.cpp)
endif()
