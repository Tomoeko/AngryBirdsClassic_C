file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7706211516c5f7503ee9dc44eb3bf8a01cbd66a36a083a3276e49148a99b5ba0")
    message(FATAL_ERROR "Expected the recorded Android time-of-impact corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android impact comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
