file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "f3d4e2d173f3f28d0bc7474e0777c62524cc4af27417383766485341d5ea6e0e")
    message(FATAL_ERROR "Expected the recorded original powerup used-list corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Powerup use comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
