cmake_minimum_required(VERSION 3.22)
project(SimpleDynamicEQ VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# JUCE â€“ clone if not present
# -----------------------------------------------------------------------
# JUCE bundled locally

add_subdirectory(JUCE)

# -----------------------------------------------------------------------
# Plugin target
# -----------------------------------------------------------------------
juce_add_plugin(ProEQ
    COMPANY_NAME         "GoriSound"
    PLUGIN_MANUFACTURER_CODE "GrSd"
    PLUGIN_CODE          "PrEQ"
    FORMATS              VST3 Standalone
    PRODUCT_NAME         "SimpleDynamicEQ"
    IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES      "Fx" "EQ"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(ProEQ)

juce_add_binary_data(ProEQ_BinaryData
    SOURCES
        resources/background.png
)

target_sources(ProEQ PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/DSP/EQBand.cpp
    src/DSP/BiquadFilter.cpp
    src/DSP/DynamicEQ.cpp
    src/DSP/MidSideProcessor.cpp
    src/DSP/LinearPhaseEQ.cpp
    src/DSP/SpectrumAnalyzer.cpp
    src/DSP/FrequencyProblemDetector.cpp
    src/GUI/EQCurveComponent.cpp
    src/GUI/BandHandle.cpp
    src/GUI/AnalyzerComponent.cpp
    src/GUI/KnobLookAndFeel.cpp
    src/GUI/BandControlPanel.cpp
    src/GUI/AudioFilePlayer.cpp
)

target_include_directories(ProEQ PRIVATE src)

target_compile_definitions(ProEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ProEQ
    PRIVATE
        ProEQ_BinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Generate linker map file for crash analysis
if (MSVC)
    target_link_options(ProEQ PRIVATE /MAP /Zi)
    target_compile_options(ProEQ PRIVATE /Zi)
endif()

