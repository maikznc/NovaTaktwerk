cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel), runs on macOS 11 and newer
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

# Windows: link the C++ runtime statically, so the plugin runs without extra installers
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(NovaTaktwerk VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------------------------
# JUCE: either a local copy (-DNOVA_JUCE_DIR=/path/to/JUCE) or downloaded automatically
set(NOVA_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
option(NOVA_COPY_PLUGIN_AFTER_BUILD "Install the plugin into the system plug-in folders after building" OFF)
option(NOVA_BUILD_TOOLS "Build the offline render test and the UI snapshot tool" OFF)
option(NOVA_LINUX_MINIMAL "Linux build without X11 extensions / ALSA (for headless test machines)" OFF)

if(NOVA_JUCE_DIR)
    add_subdirectory("${NOVA_JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------------------------
# Brand and plug-in identity. Keep COMPANY_NAME and PLUGIN_MANUFACTURER_CODE identical across all
# NOVA plug-ins, so DAWs list them together. PLUGIN_CODE must be unique for every plug-in.
set(NOVA_COMPANY        "NOVA")
set(NOVA_PRODUCT        "NOVA Taktwerk")
set(NOVA_MANUFACTURER   "Nova")     # 4 characters, at least one upper-case letter
set(NOVA_PLUGIN_CODE    "Tkwk")     # 4 characters, unique per plug-in

set(NOVA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NOVA_FORMATS AU)
endif()

juce_add_plugin(NovaTaktwerk
    COMPANY_NAME                "${NOVA_COMPANY}"
    PRODUCT_NAME                "${NOVA_PRODUCT}"
    BUNDLE_ID                   "com.nova.taktwerk"
    PLUGIN_MANUFACTURER_CODE    ${NOVA_MANUFACTURER}
    PLUGIN_CODE                 ${NOVA_PLUGIN_CODE}
    FORMATS                     ${NOVA_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Drum
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD     ${NOVA_COPY_PLUGIN_AFTER_BUILD})

set(NOVA_DSP_SOURCES
    Source/DSP/DrumSynth.cpp
    Source/DSP/DrumTrack.cpp
    Source/DSP/MasterFx.cpp)

set(NOVA_PLUGIN_SOURCES
    ${NOVA_DSP_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NovaLookAndFeel.cpp
    Source/UI/PadComponent.cpp
    Source/UI/TrackEditorPanel.cpp
    Source/UI/FxPanel.cpp)

target_sources(NovaTaktwerk PRIVATE ${NOVA_PLUGIN_SOURCES})

set(NOVA_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(NOVA_LINUX_MINIMAL)
    list(APPEND NOVA_COMMON_DEFINITIONS
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XINPUT=0 JUCE_USE_XRANDR=0
        JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_compile_definitions(NovaTaktwerk PUBLIC ${NOVA_COMMON_DEFINITIONS})

# The source files contain German text (UTF-8): tell MSVC so umlauts come out right
if(MSVC)
    target_compile_options(NovaTaktwerk PRIVATE /utf-8)
endif()

target_link_libraries(NovaTaktwerk
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------------------------
# Developer tools (not needed to use the plug-in)
if(NOVA_BUILD_TOOLS)
    # Renders all 12 voices and a short groove to a WAV file and checks for clipping / NaNs
    juce_add_console_app(NovaRenderTest PRODUCT_NAME "NovaRenderTest")
    target_sources(NovaRenderTest PRIVATE Tools/RenderTest.cpp ${NOVA_DSP_SOURCES})
    target_compile_definitions(NovaRenderTest PRIVATE JUCE_STANDALONE_APPLICATION=1 ${NOVA_COMMON_DEFINITIONS})
    target_link_libraries(NovaRenderTest PRIVATE juce::juce_audio_formats juce::juce_audio_basics juce::juce_core
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # Draws the plug-in window into PNG files (pads and the open sound editor)
    juce_add_console_app(NovaSnapshot PRODUCT_NAME "NovaSnapshot")
    target_sources(NovaSnapshot PRIVATE Tools/Snapshot.cpp ${NOVA_PLUGIN_SOURCES})
    target_compile_definitions(NovaSnapshot PRIVATE JUCE_STANDALONE_APPLICATION=1 JucePlugin_Name="NOVA Taktwerk"
                               ${NOVA_COMMON_DEFINITIONS})
    target_link_libraries(NovaSnapshot PRIVATE juce::juce_audio_utils juce::juce_audio_formats
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
