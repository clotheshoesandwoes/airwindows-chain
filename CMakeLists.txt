cmake_minimum_required(VERSION 3.22)

# Static runtime, matching the airwindows registry library.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AirwindowsChain VERSION 0.2.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AWCHAIN_BUILD_HARNESS "Build the headless test harness" ON)

# Every Airwindows effect plus its documentation (MIT).
add_subdirectory(libs/airwin2rack EXCLUDE_FROM_ALL)

add_subdirectory(libs/JUCE)
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

juce_add_binary_data(awchain_assets
    HEADER_NAME AwChainAssets.h
    NAMESPACE AwChainAssets
    SOURCES
        resources/fonts/IBMPlexSans-Regular.ttf
        resources/fonts/IBMPlexSans-Medium.ttf
        resources/fonts/IBMPlexSans-SemiBold.ttf
        resources/fonts/IBMPlexMono-Regular.ttf)

set(AWCHAIN_SOURCES
    src/Catalog.cpp
    src/ChainProcessor.cpp
    src/ChainEditor.cpp
    src/ui/Theme.cpp
    src/ui/ChainList.cpp
    src/ui/SlotPanel.cpp
    src/ui/Picker.cpp)

set(AWCHAIN_DEFINITIONS
    AWCHAIN_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    _USE_MATH_DEFINES=1)

juce_add_plugin(AirwindowsChain
    PRODUCT_NAME "Airwindows Chain"
    COMPANY_NAME "Kani"
    BUNDLE_ID "com.kani.airwindowschain"
    PLUGIN_MANUFACTURER_CODE Kani
    PLUGIN_CODE AwCh
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    FORMATS VST3)

target_sources(AirwindowsChain PRIVATE ${AWCHAIN_SOURCES} src/PluginEntry.cpp)
target_include_directories(AirwindowsChain PRIVATE src)
target_compile_definitions(AirwindowsChain PUBLIC ${AWCHAIN_DEFINITIONS})
target_link_libraries(AirwindowsChain
    PRIVATE
        airwin-registry
        awchain_assets
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET AirwindowsChain
    CLAP_ID "com.kani.airwindows-chain"
    CLAP_FEATURES "audio-effect" "multi-effects" "stereo" "free and open source")

if (AWCHAIN_BUILD_HARNESS)
    # A console app that compiles the same processor and editor, so the chain
    # can be tested and the UI rendered to PNG without a DAW.
    juce_add_console_app(awchain_harness PRODUCT_NAME "awchain_harness")
    target_sources(awchain_harness PRIVATE ${AWCHAIN_SOURCES} tests/Harness.cpp)
    target_include_directories(awchain_harness PRIVATE src)
    target_compile_definitions(awchain_harness PRIVATE
        ${AWCHAIN_DEFINITIONS}
        JUCE_PLUGINHOST_VST3=1
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0)
    target_link_libraries(awchain_harness
        PRIVATE
            airwin-registry
            awchain_assets
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
