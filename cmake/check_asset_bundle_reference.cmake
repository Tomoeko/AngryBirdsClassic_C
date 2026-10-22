file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b834ffe8aaf0ea5bdb07cd4f3dd0f2b89142d6034e64458c184aacb50239190e")
    message(FATAL_ERROR "Expected the recorded original asset-bundle corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original asset-bundle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
