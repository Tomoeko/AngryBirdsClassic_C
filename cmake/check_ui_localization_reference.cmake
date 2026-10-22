file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "318f52a80085de92c0facdb0df24bf75764bcc5a03af40e7a05963a6be9a1cf5")
    message(FATAL_ERROR "Expected the recorded original localization helper corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original localization comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
