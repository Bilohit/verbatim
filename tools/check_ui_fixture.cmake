# Regenerates the UI fixture with vb_spec and fails if it differs from the committed copy.
execute_process(COMMAND ${VB_SPEC} --ui-fixture ${OUT} RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "vb_spec --ui-fixture failed")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT} ${EXPECTED} RESULT_VARIABLE diff)
if(NOT diff EQUAL 0)
    message(FATAL_ERROR "gui/tests/fixtures/core.json is stale: run vb_spec --ui-fixture gui/tests/fixtures/core.json")
endif()
