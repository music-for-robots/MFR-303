cmake_minimum_required(VERSION 3.22)
project(MFR303 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Offline DSP test (no JUCE needed)
add_executable(SquelchTest tools/SquelchTest.cpp)

option(SQUELCH_BUILD_PLUGIN "Build the plugin" ON)
if(SQUELCH_BUILD_PLUGIN)
    add_subdirectory(JUCE)

    juce_add_plugin(Squelch
        COMPANY_NAME "Music For Robots"
        PLUGIN_MANUFACTURER_CODE MfRb
        PLUGIN_CODE Sq03
        BUNDLE_ID com.musicforrobots.mfr303
        FORMATS VST3 Standalone
        PRODUCT_NAME "MFR-303"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Instrument Synth
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(Squelch PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Squelch PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(Squelch
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
