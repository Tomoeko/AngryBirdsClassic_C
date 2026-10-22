file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "519878db9ebe5dd91257eddbe87ff3c4731298b152e59df003f92512fe3b42f2")
    message(FATAL_ERROR "Expected the recorded original bird-retirement corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original bird-retirement comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
