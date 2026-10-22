file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5a7adfb4287c82096e3c2598430a4de5e6064a797d4fc091705eac6654d74c30")
    message(FATAL_ERROR "Expected the recorded original integer-order corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original integer order comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
