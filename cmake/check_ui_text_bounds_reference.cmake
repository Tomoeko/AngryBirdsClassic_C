file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "4f9427108cd6cf9b695e0b85ae20dbbdea307398b9bd0149f6bbea1bdb03575a")
    message(FATAL_ERROR "Expected the recorded original Text bounds/correction corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Text bounds comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
