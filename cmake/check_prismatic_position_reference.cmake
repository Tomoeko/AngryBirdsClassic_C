file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0ac461bc676b8df816a019ea7d9bb865432fd7bc4ee11eb7de8f920509a3c48b")
    message(FATAL_ERROR "Expected the recorded native prismatic position corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native prismatic position comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
