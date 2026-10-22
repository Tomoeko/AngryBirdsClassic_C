file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2097042232e4d6b509fc191b3f415adfe750acf8a14ee248339b41a6e6d4001c")
    message(FATAL_ERROR "Expected the recorded original SettingsWrapper corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Statistics comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
