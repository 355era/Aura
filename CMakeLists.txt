cmake_minimum_required(VERSION 3.22)

project(Aura VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Aura
    COMPANY_NAME "355ERA"
    BUNDLE_ID "com.355era.aura"
    PLUGIN_MANUFACTURER_CODE ERAA
    PLUGIN_CODE AuR1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Aura"
    DESCRIPTION "FFT spectral shifter for botanical sound design"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Aura)

target_sources(Aura PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_features(Aura PUBLIC cxx_std_17)

target_compile_definitions(Aura PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Aura PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
