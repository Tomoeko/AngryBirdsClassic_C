file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2e36ea1fb7731e8247614a0451d6907b8c7b0f4f27fa39aa5a52a8d20cea7531")
    message(FATAL_ERROR "Expected the original float Lua sine/cosine corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original script trigonometry comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
