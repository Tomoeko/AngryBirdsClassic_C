file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "942c60639cbb78d9c1f41e1f9558e63ac786accbb2f883cc587f9cfacaacafe0")
    message(FATAL_ERROR "Expected the recorded original character animation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Character animation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
