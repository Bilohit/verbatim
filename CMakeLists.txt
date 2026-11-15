cmake_minimum_required(VERSION 3.24)

project(Verbatim VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Determinism contract C7 (docs/02-spec.md): precise floating point, no contraction, never fast-math.
if(MSVC)
    add_compile_options(/fp:precise /utf-8)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(/clang:-ffp-contract=off)
    endif()
else()
    add_compile_options(-ffp-contract=off -fno-fast-math)
endif()

include(FetchContent)

# Non-Windows CI jobs build only core and its tests, without JUCE.
option(VERBATIM_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(VERBATIM_BUILD_TESTS "Build the test suites" ON)

add_subdirectory(core)
add_subdirectory(tools)
if(VERBATIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(NOT VERBATIM_BUILD_PLUGIN)
    return()
endif()

# JUCE is pinned to one 8.0.x tag (docs/02-spec.md A4). Changing it needs the owner's approval.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Verbatim
    COMPANY_NAME             "Verbatim Audio"
    BUNDLE_ID                com.verbatimaudio.verbatim
    PLUGIN_MANUFACTURER_CODE Vrbm
    PLUGIN_CODE              Vbt1
    FORMATS                  VST3
    PRODUCT_NAME             "Verbatim"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    VST3_CATEGORIES          Fx
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(Verbatim PRIVATE
    plugin/VerbatimProcessor.cpp)

target_compile_definitions(Verbatim PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Verbatim
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
