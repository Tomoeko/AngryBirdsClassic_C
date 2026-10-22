file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "787460b8a7d005e8116fc551aae655efded7d3c001bbd6282c0c5b4ff82278bf")
    message(FATAL_ERROR "Expected the recorded native zero-radius circle corpus")
endif()
execute_process(COMMAND "${CHECKER}" --circle-zeros "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native zero-radius comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
