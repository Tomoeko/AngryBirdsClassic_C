file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "dbc9da9527bc48fae7f13af1b23c54b3406df736846202ef1a00fb5fa0ebd348")
    message(FATAL_ERROR "Expected the recorded native game joint registry/glue-policy corpus")
endif()
execute_process(COMMAND "${CHECKER}" --glue-policy "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native glue policy comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
