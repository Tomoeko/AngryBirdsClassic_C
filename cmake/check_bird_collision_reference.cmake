file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "62bef8b39e2845c8dfe2e75b8130d4a907f2d619c554c829937c0f91f8dbd67b")
    message(FATAL_ERROR "Expected the recorded original coupled bird callback corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Coupled bird callback comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
