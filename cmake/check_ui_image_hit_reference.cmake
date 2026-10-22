file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "72004be3f53da97ebc88d7a9ee8a58e4f945c723e50d7d5b56ffad5417d5a16b")
    message(FATAL_ERROR "Expected the recorded original ImageButton hit corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original ImageButton comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
