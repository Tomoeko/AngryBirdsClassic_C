file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6d024215708d735d78d36a45889420e4bc9dc6fefe399c33e416fc481cd87083")
    message(FATAL_ERROR "Expected the recorded original menu link corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original menu link comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
