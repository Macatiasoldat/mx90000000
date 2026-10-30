cmake_minimum_required(VERSION 3.22)
# macOS : binaire universel (Apple Silicon M1 + Intel), compatible macOS 11+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Jovian8 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime Visual C++ lié statiquement : pas besoin d'installer de redistribuable sur la machine de musique
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(JOVIAN_FORMATS VST3 AU Standalone)   # AU = format lu par GarageBand / Logic
else()
    set(JOVIAN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Jovian8
    COMPANY_NAME "KoalaAudio"
    PLUGIN_MANUFACTURER_CODE Koal
    PLUGIN_CODE Jov8
    FORMATS ${JOVIAN_FORMATS}
    PRODUCT_NAME "Jovian8"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Jovian8 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Jovian8 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Jovian8
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- Sampler16 (sampler 16 pads + séquenceur)
juce_add_plugin(Sampler16
    COMPANY_NAME "KoalaAudio"
    PLUGIN_MANUFACTURER_CODE Koal
    PLUGIN_CODE Smp6
    FORMATS ${JOVIAN_FORMATS}
    PRODUCT_NAME "Sampler16"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Sampler16 PRIVATE
    Sampler/Source/SamplerProcessor.cpp
    Sampler/Source/SamplerEditor.cpp)

target_compile_definitions(Sampler16 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(Sampler16
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
