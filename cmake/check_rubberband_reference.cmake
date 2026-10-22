file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "e62bde016ef6bd70aaf2a3f2f7a53457d354ed620c4f9dc0dc38a1b01e4be958")
    message(FATAL_ERROR "Expected the pinned native rubberband image vertices")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native rubberband comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
