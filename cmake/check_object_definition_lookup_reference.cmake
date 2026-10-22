file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "82ef40d273a39621d6f39569f8448f575e170e8500be0bc650df3797d0a77d9b")
    message(FATAL_ERROR "Expected the recorded original getObjectDefinition corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Object definition comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
