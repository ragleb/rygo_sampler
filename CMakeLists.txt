cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
# Static runtime: the plugin has to load on a PC without the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
option(RYGO_UNIVERSAL "Build for both Apple Silicon and Intel" ON)
option(RYGO_BUILD_TESTS "Build the engine tests and the plugin host check" ON)

if(APPLE AND RYGO_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(Rygostancija VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory(third_party/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(RYGO_SOURCES
    source/ArtStore.cpp
    source/Parameters.cpp
    source/SampleLibrary.cpp
    source/SamplerEngine.cpp
    source/FxChain.cpp
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/UiComponents.cpp)

function(rygo_configure target)
    target_sources(${target} PRIVATE ${RYGO_SOURCES})
    target_include_directories(${target} PRIVATE source)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_FLAC=1
        JUCE_USE_OGGVORBIS=1
        JUCE_USE_MP3AUDIOFORMAT=0)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(Rygostancija
    PRODUCT_NAME "Rygostancija"
    COMPANY_NAME "ZHRI Records"
    DESCRIPTION "Sample instrument with built-in effects"
    BUNDLE_ID "com.zhrirecords.rygostancija"
    PLUGIN_MANUFACTURER_CODE ZHRI
    PLUGIN_CODE Rygo
    FORMATS AU VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    # Without this flag the VST3 wrapper makes the editor ignore key presses on
    # macOS, and numeric values could not be typed into the knobs.
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

rygo_configure(Rygostancija)

# Factory banks and the art live inside every plugin bundle, so installing the
# plugin is all a user has to do: the samples can never go missing.
set(RYGO_FACTORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/factory_samples")
set(RYGO_ART_DIR "${CMAKE_CURRENT_SOURCE_DIR}/art")
foreach(format AU VST3 Standalone)
    set(plugin_target Rygostancija_${format})
    # JUCE creates only the formats a platform supports: there is no AU target on Windows.
    if(NOT TARGET ${plugin_target})
        continue()
    endif()
    add_custom_command(TARGET ${plugin_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${plugin_target}>/../Resources/Samples"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RYGO_FACTORY_DIR}" "$<TARGET_FILE_DIR:${plugin_target}>/../Resources/Samples"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${plugin_target}>/../Resources/Art"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RYGO_ART_DIR}" "$<TARGET_FILE_DIR:${plugin_target}>/../Resources/Art"
        VERBATIM)
    if(APPLE)
        # Finder tags and download marks on the art would make codesign refuse the bundle.
        add_custom_command(TARGET ${plugin_target} POST_BUILD
            COMMAND /usr/bin/xattr -cr "$<TARGET_FILE_DIR:${plugin_target}>/../.."
            COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_FILE_DIR:${plugin_target}>/../.."
            VERBATIM)
    endif()
endforeach()

if(RYGO_BUILD_TESTS)
    juce_add_console_app(RygoTests PRODUCT_NAME "RygoTests")
    rygo_configure(RygoTests)
    target_sources(RygoTests PRIVATE tests/RygoTests.cpp tests/ArtGenerator.cpp)
    target_compile_definitions(RygoTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    juce_add_console_app(RygoHostCheck PRODUCT_NAME "RygoHostCheck")
    target_sources(RygoHostCheck PRIVATE tests/RygoHostCheck.cpp)
    target_compile_definitions(RygoHostCheck PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_PLUGINHOST_AU=1
        JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(RygoHostCheck PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
