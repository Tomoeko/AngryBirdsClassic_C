file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b7f016b3c17d864b7b52132b787c539e4d0d903c9d36d2d93b26bd5b3a9c394e")
    message(FATAL_ERROR "Expected the recorded original text visibility corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original visibility comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
