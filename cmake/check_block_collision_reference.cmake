file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d0b731a1a4ec4edaddb5bf03ec4007b4e55a0dd97b2e4f12f28f980a592fa10f")
    message(FATAL_ERROR "Expected the recorded original block-collision callback corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Block-collision callback comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
