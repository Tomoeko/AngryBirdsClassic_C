file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2131e68da8563a9153370cb784ec08e0369316e9b0bb283e621ae44bf7db66b3")
    message(FATAL_ERROR "Expected the recorded original VP8L bit-field corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8L bit-field comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
