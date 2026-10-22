file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6cb59331e51f2cbdef01243169f388d2dc6b31328edbbabe1297aeaf76008cba")
    message(FATAL_ERROR "Expected the recorded native world force-clear corpus")
endif()
execute_process(COMMAND "${CHECKER}" --clear-forces "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native force-clear comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
