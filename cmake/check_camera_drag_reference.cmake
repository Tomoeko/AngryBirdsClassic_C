file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "bacfcd9d29f24410e940efe5712b0f2ec83ca5e2da98d9c8ce8cba9103b8db3b" AND
    NOT reference_hash STREQUAL
    "0f0f8e774e8fe7eac94ccbcba3ad52240bd526b646036152dc6c50611182a36e")
    message(FATAL_ERROR "Expected the original camera drag command corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera drag comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
