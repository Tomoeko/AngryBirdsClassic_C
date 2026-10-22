file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "9a9fafc0e3cc76c534fcdfde0c7f79f93070de2f3bd5b7698d359f5f85f6f9db")
    message(FATAL_ERROR "Expected the original VP8 coefficient block corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 coefficient comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
