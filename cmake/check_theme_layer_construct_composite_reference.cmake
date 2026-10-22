file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "443a3df44f653b1159ba9ede94cb620c68242b99aded1d90c01859674afa6ae0")
    message(FATAL_ERROR "Expected original ThemeLayer composite metadata constructors")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original composite constructor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
