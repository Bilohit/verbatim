FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

include(Catch)

add_executable(verbatim_core_tests
    core/primitives.test.cpp
    core/parse.test.cpp
    core/generate.test.cpp
    core/golden.test.cpp)
target_link_libraries(verbatim_core_tests PRIVATE verbatim_core Catch2::Catch2WithMain)
target_compile_definitions(verbatim_core_tests PRIVATE VERBATIM_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
catch_discover_tests(verbatim_core_tests)

# G4 guard script self-test.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME G4_golden_guard_selftest COMMAND Python3::Interpreter -I ${PROJECT_SOURCE_DIR}/tools/check_golden.py --self-test)
endif()

# T02 acceptance: core/ has no JUCE include and no floating-point type.
add_test(NAME core_no_float_no_juce
    COMMAND ${CMAKE_COMMAND} -DDIR=${PROJECT_SOURCE_DIR}/core -P ${PROJECT_SOURCE_DIR}/tools/check_core_pure.cmake)

# vb_spec CLI: prints the chain hash of worked example 1, decodes \xNN escapes (example 8).
add_test(NAME vb_spec_hello COMMAND vb_spec "hello")
set_tests_properties(vb_spec_hello PROPERTIES PASS_REGULAR_EXPRESSION "chainHash 692ac662981b2247")
add_test(NAME vb_spec_escapes
    COMMAND vb_spec [=[\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xD0\xBC\xD0\xB8\xD1\x80 \xE2\x9C\x89]=])
set_tests_properties(vb_spec_escapes PROPERTIES PASS_REGULAR_EXPRESSION "chainHash b70518222f8d15f0")

if(VERBATIM_BUILD_DSP)
    add_executable(verbatim_dsp_tests
        ${VERBATIM_DSP_SOURCES}
        dsp/harness.cpp
        dsp/engine.test.cpp
        dsp/lane.test.cpp
        dsp/feedback.test.cpp
        dsp/mod.test.cpp
        dsp/timebase.test.cpp
        dsp/effects.test.cpp
        dsp/filter.test.cpp
        dsp/calibration.test.cpp
        dsp/fuzz.test.cpp)
    target_include_directories(verbatim_dsp_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/dsp)
    target_compile_definitions(verbatim_dsp_tests PRIVATE
        VERBATIM_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
        JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(verbatim_dsp_tests PRIVATE
        verbatim_core juce::juce_dsp juce::juce_audio_basics juce::juce_recommended_config_flags
        Catch2::Catch2WithMain)
    catch_discover_tests(verbatim_dsp_tests)
endif()

if(VERBATIM_BUILD_PLUGIN)
    # Links the plugin's shared-code target, which holds the processor and its JUCE modules.
    add_executable(verbatim_plugin_tests plugin/state.test.cpp plugin/editor.test.cpp plugin/transport.test.cpp)
    target_include_directories(verbatim_plugin_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/dsp
        $<TARGET_PROPERTY:Verbatim,INCLUDE_DIRECTORIES>)
    target_compile_definitions(verbatim_plugin_tests PRIVATE $<TARGET_PROPERTY:Verbatim,COMPILE_DEFINITIONS>)
    target_link_libraries(verbatim_plugin_tests PRIVATE Verbatim Catch2::Catch2WithMain)
    catch_discover_tests(verbatim_plugin_tests)
endif()
