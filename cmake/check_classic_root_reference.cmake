file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "c0eff16127d921854e326b71d612f41393276d16bd7ec2361b226e60385ba56c")
    message(FATAL_ERROR "Expected the recorded original root-update corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Root-update comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
