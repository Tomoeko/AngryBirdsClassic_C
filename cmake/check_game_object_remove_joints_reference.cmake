file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "519a298b44845ed3ad87880ed426375046fe095aaad79db44505fab1ecd48286")
    message(FATAL_ERROR "Expected the recorded original object/joint removal corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object/joint removal comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
