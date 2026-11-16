cmake_minimum_required(VERSION 3.24)

project(Verbatim VERSION 1.0.0 LANGUAGES C CXX)

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

# Core-only CI jobs (linux-gcc, macos-clang) turn DSP and plugin off and never fetch JUCE.
option(VERBATIM_BUILD_DSP "Build the DSP engine and its tests (fetches JUCE)" ON)
option(VERBATIM_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(VERBATIM_BUILD_TESTS "Build the test suites" ON)

add_subdirectory(core)
add_subdirectory(tools)

if(VERBATIM_BUILD_DSP OR VERBATIM_BUILD_PLUGIN)
    # JUCE is pinned to one 8.0.x tag (docs/02-spec.md A4). Changing it needs the owner's approval.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.17-security
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(dsp)
endif()

if(VERBATIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(NOT VERBATIM_BUILD_PLUGIN)
    return()
endif()

# WebView2 SDK (BSD-3-Clause) from NuGet into the build tree; JUCE links it statically.
FetchContent_Declare(webview2
    URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3485.44
    DOWNLOAD_NAME webview2.zip
    SOURCE_DIR ${CMAKE_BINARY_DIR}/nuget/Microsoft.Web.WebView2.1.0.3485.44)
FetchContent_MakeAvailable(webview2)
set(JUCE_WEBVIEW2_PACKAGE_LOCATION ${CMAKE_BINARY_DIR}/nuget)

# Web UI: npm ci + vite build, zipped and embedded as binary data (no network at run time).
find_program(NPM_EXECUTABLE npm REQUIRED)
if(WIN32)
    set(NPM cmd /c ${NPM_EXECUTABLE}) # npm.cmd would end a VS custom-build batch early without this
else()
    set(NPM ${NPM_EXECUTABLE})
endif()
file(GLOB_RECURSE VERBATIM_GUI_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/gui/src/*
    ${PROJECT_SOURCE_DIR}/gui/index.html
    ${PROJECT_SOURCE_DIR}/gui/package-lock.json
    ${PROJECT_SOURCE_DIR}/gui/vite.config.ts)
set(VERBATIM_GUI_ZIP ${CMAKE_BINARY_DIR}/gui/gui.zip)
add_custom_command(OUTPUT ${VERBATIM_GUI_ZIP}
    COMMAND ${NPM} ci --no-audit --no-fund
    COMMAND ${NPM} run build
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gui
    COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR}/gui/dist ${CMAKE_COMMAND} -E tar cf ${VERBATIM_GUI_ZIP} --format=zip .
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/gui
    DEPENDS ${VERBATIM_GUI_SOURCES}
    COMMENT "Building web UI"
    VERBATIM)
add_custom_target(VerbatimGuiBuild DEPENDS ${VERBATIM_GUI_ZIP})
juce_add_binary_data(VerbatimGuiData NAMESPACE VerbatimGui SOURCES ${VERBATIM_GUI_ZIP})
add_dependencies(VerbatimGuiData VerbatimGuiBuild)

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
    NEEDS_WEBVIEW2           TRUE
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(Verbatim PRIVATE
    plugin/VerbatimProcessor.cpp
    plugin/State.cpp
    plugin/VerbatimEditor.cpp
    plugin/Bridge.cpp
    plugin/ParamText.cpp
    ${VERBATIM_DSP_SOURCES})
target_include_directories(Verbatim PRIVATE ${PROJECT_SOURCE_DIR})

target_compile_definitions(Verbatim PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Verbatim
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        verbatim_core
        VerbatimGuiData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
