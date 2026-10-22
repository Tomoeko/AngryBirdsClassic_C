file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "39041c9f1c078de9e4edf6927907335f6dff26932fe83a8ccfa6addc3248ae39")
    message(FATAL_ERROR "Expected the recorded Android continuous-position corpus")
endif()
execute_process(COMMAND "${CHECKER}" --toi-position "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android continuous-position comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
