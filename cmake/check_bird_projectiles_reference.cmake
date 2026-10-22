file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a309fb4ec29642b10d54652aa7ce95a89d5817f5bdfe03826c93ddd8f1408626")
    message(FATAL_ERROR "Expected the recorded original bird projectile corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird projectile comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
