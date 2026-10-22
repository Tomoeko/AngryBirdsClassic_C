file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5df5963c020cbb95a40ea9f271a5e921cef1d793d63dac301f369ee14c752928")
    message(FATAL_ERROR "Expected the recorded live native object-property corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object-property comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
