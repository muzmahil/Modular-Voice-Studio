cmake_minimum_required(VERSION 3.22)
project(ModularVoiceStudio VERSION 1.5.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------
# Expects JUCE checked out as a submodule/subdir at ./JUCE (JUCE 8.x)
add_subdirectory(JUCE)

# --- RNNoise (Neural Voice Noise Suppression) ---------------------------
add_subdirectory(Source/RNNoise)

# --- VST2 SDK (user-supplied, licensing restricted) ----------------------
# Point this at your local VST2 SDK folder (contains pluginterfaces/vst2.x/aeffect.h etc.)
set(VST2_SDK_PATH "C:/vstsdk2.4" CACHE PATH "Path to VST2 SDK" FORCE)

set(PLUGIN_FORMATS VST3 Standalone)
if(EXISTS "${VST2_SDK_PATH}")
    juce_set_vst2_sdk_path("${VST2_SDK_PATH}")
    list(APPEND PLUGIN_FORMATS VST)
    message(STATUS "VST2 SDK found at ${VST2_SDK_PATH} - Enabling VST2 target")
endif()

set(EXTRA_PLUGIN_ARGS)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon.png")
    list(APPEND EXTRA_PLUGIN_ARGS ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon.png")
endif()
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon_small.png")
    list(APPEND EXTRA_PLUGIN_ARGS ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon_small.png")
endif()

juce_add_plugin(ModularVoiceStudio
    COMPANY_NAME "Furkan (rootcf) Centek"
    COMPANY_WEBSITE "https://github.com/rootcf"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Furkan (rootcf) Centek"
    BUNDLE_ID "com.rootcf.modularvoicestudio"
    PLUGIN_MANUFACTURER_CODE Rfck
    PLUGIN_CODE Mvsd
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Modular Voice Studio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORIES Fx
    ${EXTRA_PLUGIN_ARGS}
)

target_sources(ModularVoiceStudio PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Graph/ModuleFactory.cpp
    Source/Graph/PresetManager.cpp
    Source/UI/NodeComponent.cpp
    Source/UI/NodeCanvas.cpp
    Source/UI/CanvasMinimap.cpp
    Source/UI/GlobalOscilloscopeHUD.cpp
    Source/UI/NodeInspectorPanel.cpp
    Source/UI/PreferencesModal.cpp
    Source/UITheme.cpp
    Source/UI/SidebarComponent.cpp
)

target_include_directories(ModularVoiceStudio PRIVATE Source Source/RNNoise/include)

target_compile_definitions(ModularVoiceStudio PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# 1. Dosyanın GERÇEK yolunu tanımlıyoruz
set(FONT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Source/Font/InterMedium.ttf")
set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon.png")

# 2. JUCE çökmeden önce CMake dosyayı bulabildi mi diye kontrol ediyoruz
if (NOT EXISTS "${FONT_PATH}")
    message(FATAL_ERROR "DAYI DUR! Font dosyasi bulunamadi. Aranan yol: ${FONT_PATH}")
endif()

# 3. BinaryData içine Font ve Logo ikonunu ekliyoruz
set(BINARY_DATA_SOURCES "${FONT_PATH}")
if (EXISTS "${ICON_PATH}")
    list(APPEND BINARY_DATA_SOURCES "${ICON_PATH}")
endif()
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/logo.png" AND NOT "${ICON_PATH}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/logo.png")
    list(APPEND BINARY_DATA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/logo.png")
endif()

juce_add_binary_data(ModularVoiceStudio_Data
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES ${BINARY_DATA_SOURCES}
)

target_link_libraries(ModularVoiceStudio PRIVATE
    ModularVoiceStudio_Data
    rnnoise
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    juce::juce_opengl
)

# Force MSVC to read source files and output strings as UTF-8
# Without this flag, MSVC uses the system ANSI codepage (CP-1252/CP-1254),
# which breaks all u8"..." string literals for Turkish/Japanese/Russian characters.
if(MSVC)
    target_compile_options(ModularVoiceStudio PRIVATE /utf-8)
endif()