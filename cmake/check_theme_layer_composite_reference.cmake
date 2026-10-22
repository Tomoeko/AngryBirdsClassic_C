file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e1cc8a4d2840cb7e5a8fa6aa71b33bf4a0d1fd04615efd8b3e5e8775b0a1fbac")
    message(FATAL_ERROR "Expected the recorded original theme composite corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme composite comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
