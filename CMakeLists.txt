cmake_minimum_required(VERSION 3.22)

project(EmberlineSpectralis VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(EmberlineSpectralis
    COMPANY_NAME "Emberline"
    PLUGIN_MANUFACTURER_CODE Embl
    PLUGIN_CODE Espc
    PRODUCT_NAME "Emberline Spectralis"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_add_binary_data(EmberlineAssets HEADER_NAME AssetData.h NAMESPACE AssetData SOURCES Assets/phoenix.png)

# factory presets are plain JSON files in Presets/Factory, embedded at build time
file(GLOB EMBERLINE_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Presets/Factory/*.json")
juce_add_binary_data(EmberlinePresets HEADER_NAME PresetData.h NAMESPACE PresetData SOURCES ${EMBERLINE_FACTORY_PRESETS})

target_sources(EmberlineSpectralis PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/Presets.h
    Source/PluginEditor.h
    Source/EmberLookAndFeel.cpp
    Source/PhoenixLogo.h
    Source/Tempo.h
    Source/Parameters.h
    Source/SynthVoice.h
    Source/Chaos.h
    Source/Curve.h
    Source/Envelope.h
    Source/LfoShape.h
    Source/Modulation.h
    Source/SubNoise.h
    Source/VoiceFilter.h
    Source/WavetableBank.h
    Source/WavetableOscillator.h
    Source/EmberLookAndFeel.h
    Source/EditorWidgets.h
    Source/LfoEditor.h
    Source/MeshDisplay.h
    Source/MatrixPanel.h
    Source/PresetBar.h
)

target_compile_definitions(EmberlineSpectralis PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(EmberlineSpectralis
    PRIVATE
        EmberlineAssets
        EmberlinePresets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Developer tool: validates / renders / level-calibrates presets and dumps the parameter catalogue.
#   cmake --build build --target EmberlineTools
juce_add_console_app(EmberlineTools PRODUCT_NAME "EmberlineTools")
set_target_properties(EmberlineTools PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_include_directories(EmberlineTools PRIVATE Source)
target_sources(EmberlineTools PRIVATE
    tools/PresetTool.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EmberLookAndFeel.cpp
    Source/Presets.cpp
)
target_compile_definitions(EmberlineTools PRIVATE
    JucePlugin_Name="Emberline Spectralis"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(EmberlineTools
    PRIVATE
        EmberlineAssets
        EmberlinePresets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
