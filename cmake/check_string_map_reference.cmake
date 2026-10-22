file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0fca087fbb83deec991b7a36e6d1d72b03f6abba9126560e76748e0aec573173")
    message(FATAL_ERROR "Expected the recorded original string map mutation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original string map comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
