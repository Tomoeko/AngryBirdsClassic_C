file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ff003fc9dcaa6b7b4b5d20d515c3ef26fd5e13289f540e40d9fad3814ff5b1ae")
    message(FATAL_ERROR "Expected the recorded original Image draw corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Image draw comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
