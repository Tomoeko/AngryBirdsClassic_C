file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "098f0424b1cbca59b8a3a221da030a2269964c067b80a6f8ad190faf2f04ec16")
    message(FATAL_ERROR "Expected the recorded original Android flame-apply corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native flame construction failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
