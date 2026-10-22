file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1eecdbd429f8fc6148e640b29c852af81f53d277f0ef2bd1dc9797122af6fca2")
    message(FATAL_ERROR "Expected the recorded native deferred glue/geometry corpus")
endif()
execute_process(COMMAND "${CHECKER}" --glue "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native glue constructor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
