file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "95684fc25d003e794ac2a8aa33610fd05eb6745013aaaa36b6cd4d358e1fd19c")
    message(FATAL_ERROR "Expected the recorded Android separating-axis corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android separation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
