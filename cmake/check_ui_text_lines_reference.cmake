file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "d69b5112fdc93e11cb14d15560de9d61603a3765b108e66ef1b54c7300a35ac9")
    message(FATAL_ERROR "Expected the recorded original Text.clip composition corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Text.clip composition differs: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
