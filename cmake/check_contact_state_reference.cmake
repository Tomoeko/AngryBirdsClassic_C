file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "f3573254abcfdbc447e9356c88eb15bc8486ea4d6a63a0b4c6a14b53b6fa9ced")
    message(FATAL_ERROR "Expected the recorded Android contact-state corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android contact comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
