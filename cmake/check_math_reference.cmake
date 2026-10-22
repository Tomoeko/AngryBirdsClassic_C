file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ba0eefe62768de82ed2fa830166f2426b64e03f474b31b88853da24efb49b03d" AND
   NOT reference_hash STREQUAL
    "cc5d637a4f19e335ca0398868409b92fef9a09be93815f62321674789c90edec")
    message(FATAL_ERROR "Expected a recorded Android 5.1.1 math corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android math comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
