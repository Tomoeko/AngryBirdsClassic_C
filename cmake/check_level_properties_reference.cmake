file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7769013f416a38dc3364d46756fc8ef1550f377f9702a678dacf99af2f7c246d")
    message(FATAL_ERROR "Expected the recorded original GameObject/mode property corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Level property comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
