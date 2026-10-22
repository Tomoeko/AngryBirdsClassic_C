file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "dd447e41cb83f2b0d247ddd1d2dad07a9cfd4179f51517bb327b9086aa700658" AND
   NOT reference_hash STREQUAL
    "54cea17c90fdf67e0cec276e285041c47b94b56ac61ec5d129bed691cda8b9dd")
    message(FATAL_ERROR "Expected the recorded original camera limit corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera limits comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
