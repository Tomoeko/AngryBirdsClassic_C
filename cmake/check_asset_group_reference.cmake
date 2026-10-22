file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1e90fbf0eaf4dc936d97c4da2f2f825c35cceb4454c371ed5117424736d6e7e0")
    message(FATAL_ERROR "Expected the recorded original asset-group management corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original asset-group comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
