file(SHA256 "${REFERENCE}" digest)
if(NOT digest STREQUAL
    "ded27318b8e86350cfd114ab6c0e8a3dd77ef9a8b607261b0b52375b44303469")
    message(FATAL_ERROR "Expected the recorded pinned Android rectangle-theme corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native rectangle-theme comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
