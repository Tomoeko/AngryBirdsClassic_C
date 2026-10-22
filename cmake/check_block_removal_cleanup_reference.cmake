file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "699e2c89789087e71d2a0ed345acd866281e810761040be03eec478a57208b05")
    message(FATAL_ERROR "Expected the recorded original removal cleanup corpus")
endif()
if(COMPOSE)
    set(extra_arguments --pass)
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" ${extra_arguments}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal cleanup comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
