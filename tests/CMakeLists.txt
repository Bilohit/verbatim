FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

include(Catch)

add_executable(verbatim_core_tests
    core/primitives.test.cpp
    core/parse.test.cpp
    core/generate.test.cpp)
target_link_libraries(verbatim_core_tests PRIVATE verbatim_core Catch2::Catch2WithMain)
catch_discover_tests(verbatim_core_tests)

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
        dsp/engine.test.cpp)
    target_include_directories(verbatim_dsp_tests PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/dsp)
    target_compile_definitions(verbatim_dsp_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(verbatim_dsp_tests PRIVATE
        verbatim_core juce::juce_dsp juce::juce_audio_basics juce::juce_recommended_config_flags
        Catch2::Catch2WithMain)
    catch_discover_tests(verbatim_dsp_tests)
endif()
