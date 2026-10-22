file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "98c43d4829877de592c63c4519bd917807c416b9b21b7cde2e3a9e09aa4d6672")
    message(FATAL_ERROR "Expected the recorded native GameLua contact corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native begin-contact comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
