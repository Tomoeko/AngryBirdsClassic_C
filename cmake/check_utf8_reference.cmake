file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "8616013f092e8114afdfed64602cc11a133183c96171faf2fa076506fe3fbb11")
    message(FATAL_ERROR "Expected the recorded native UTF-8 corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native UTF-8 comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
