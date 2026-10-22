file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "15597b564cabf0a20e4e4c1b55c146386d78e393429fb1af76ed0213745d66fb")
    message(FATAL_ERROR "Expected the recorded original Android ambient-animation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native ambient comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
