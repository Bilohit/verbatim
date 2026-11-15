# verbatim_core: text -> ChainSpec. Pure C++20, integer only, no JUCE (docs/02-spec.md C2).
add_library(verbatim_core STATIC
    Tokenizer.cpp
    Parse.cpp
    Generate.cpp
    Serialize.cpp)
target_include_directories(verbatim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/third_party)
