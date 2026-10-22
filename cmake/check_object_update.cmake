file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "16792e53b52f10785884b9d729080f3e4e92f2f6afe78830a0f2eeed45660a8c")
    message(FATAL_ERROR "Expected the recorded original Android object-update corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object-update comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
