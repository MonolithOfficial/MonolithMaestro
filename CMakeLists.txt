cmake_minimum_required(VERSION 3.22)

project(MonolithMaestro VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add JUCE as a subdirectory
# Make sure you have JUCE in a 'JUCE' folder at the root level
# You can get it via: git clone https://github.com/juce-framework/JUCE.git
add_subdirectory(JUCE)

# Define plugin manufacturer and plugin codes
set(PLUGIN_MANUFACTURER_CODE Mnlt)  # 4-character code
set(PLUGIN_CODE MnMa)                # 4-character unique plugin code

# Create the VST3 plugin
juce_add_plugin(MonolithMaestro
    COMPANY_NAME "MonolithMaestro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE ${PLUGIN_MANUFACTURER_CODE}
    PLUGIN_CODE ${PLUGIN_CODE}
    FORMATS VST3
    PRODUCT_NAME "Monolith Maestro"
    VST3_CATEGORIES Fx
)

# Add source files
target_sources(MonolithMaestro
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PitchDetector.cpp
        Source/PitchDetector.h
)

# Link required JUCE modules
target_link_libraries(MonolithMaestro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compiler definitions
target_compile_definitions(MonolithMaestro
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
