file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "fa65aab65f59989fbe78e0558bc512b8170b83017256e5e046d812e3e226a529")
    message(FATAL_ERROR "Expected the recorded original shot camera sequence corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Shot camera sequence comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
