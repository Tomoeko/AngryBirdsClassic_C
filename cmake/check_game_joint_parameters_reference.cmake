file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "f7ec83f9f97cbed23b663df991f9fecead18e537e684d907412371377515ad0e")
    message(FATAL_ERROR "Expected the recorded native joint-parameter update corpus")
endif()
execute_process(COMMAND "${CHECKER}" --parameters "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native parameter-update comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
