# DSP sources are compiled into each consumer (plugin, dsp tests) together with its own JUCE modules.
set(VERBATIM_DSP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lane.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Module.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mod.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/Sat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fx/Delay.cpp
    PARENT_SCOPE)
