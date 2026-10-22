file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "30040e8d8ad89fbe032966540bd7e65385218b36aabd3b892d8d6d0b1911778c")
    message(FATAL_ERROR "Expected the recorded original Android block-update corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native block-update comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
