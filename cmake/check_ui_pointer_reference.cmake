file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b83b9ffc5111c9e70c589cf6fcdb6559b2e80e41dc909f8a8627aa34bfb29822")
    message(FATAL_ERROR "Expected the recorded original UI pointer corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original UI pointer comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
