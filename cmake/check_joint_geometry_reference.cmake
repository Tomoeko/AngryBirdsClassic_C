file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "997af7935e912845fc9c682d7d32928f8ced68f5f9e399e9f385d8ec3151c1e0")
    message(FATAL_ERROR "Expected the recorded native joint geometry corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native joint geometry comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
