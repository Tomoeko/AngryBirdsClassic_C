file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "43fe12c6936b13abf0ede163ae7fb3176d7640bdcf2a8ac73c9880763b7bb539")
    message(FATAL_ERROR "Expected the recorded original theme-asset corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme-asset comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
