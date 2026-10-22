file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ffa515f0caa75d9750cc40f0c17c87fb82d2d6ddc092148e4d9ccf05a57ae71e")
    message(FATAL_ERROR "Expected the recorded native GameObject triangle corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native triangle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
