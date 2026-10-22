file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "c724ef5f0a2024cded7456baef7c6d4d90227d2cf5026e35818c5082ea7586ab")
    message(FATAL_ERROR "Expected the recorded original-Lua block damage corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original-Lua block damage comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
