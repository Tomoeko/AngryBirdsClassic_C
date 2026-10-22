file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0c3c20ee531be37f241b8c9fe279157b35188a9a1bafcf61e278ed9c3d6f2236")
    message(FATAL_ERROR "Expected the original camera shake command corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera shake comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
