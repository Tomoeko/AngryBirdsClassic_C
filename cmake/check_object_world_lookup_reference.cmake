file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "8f633f2c362c7f7839e7ad6c4a80c99b6b9815d340321d2f9e1071af303639a2")
    message(FATAL_ERROR "Expected the recorded original getObject corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "World object comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
