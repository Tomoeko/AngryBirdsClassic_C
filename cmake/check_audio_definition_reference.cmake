file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "4694a7b169d456f590a79d03e93f86a5c7a3b2ddebf080af8b60513ae8749535")
    message(FATAL_ERROR "Expected the original createAudio definition reference")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native audio definition comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
