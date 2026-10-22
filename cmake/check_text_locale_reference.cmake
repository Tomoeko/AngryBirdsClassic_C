file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e74366b77a9db953288301cfc9679f953c4906c220af9a9514cb5601d5a115ec")
    message(FATAL_ERROR "Expected the recorded Android GameLua locale corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original locale comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
