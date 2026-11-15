FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

include(Catch)

add_executable(verbatim_core_tests
    core/primitives.test.cpp
    core/parse.test.cpp)
target_link_libraries(verbatim_core_tests PRIVATE verbatim_core Catch2::Catch2WithMain)
catch_discover_tests(verbatim_core_tests)

# T02 acceptance: core/ has no JUCE include and no floating-point type.
add_test(NAME core_no_float_no_juce
    COMMAND ${CMAKE_COMMAND} -DDIR=${PROJECT_SOURCE_DIR}/core -P ${PROJECT_SOURCE_DIR}/tools/check_core_pure.cmake)
