file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e0850efb89e6ad6c6fc2d1b9d52907ee94dc5da6d569671ae8db8fb9bd5f5f10")
    message(FATAL_ERROR "Expected the recorded native revolute position corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native revolute position comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
