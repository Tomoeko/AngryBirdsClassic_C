file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "79a0f91bd6385b273df389dfc429bd33e6bffbd80ae7209e3920f15281e615c2")
    message(FATAL_ERROR "Expected the recorded original native value-text corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" native
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native value-text comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
