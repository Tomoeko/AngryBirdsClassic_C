file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "8c9ead3a49515dd75defb24160946b5e6dd294653d8bf4cde1853735ec1c1565")
    message(FATAL_ERROR "Expected the recorded original reload geometry corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Reload geometry comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
