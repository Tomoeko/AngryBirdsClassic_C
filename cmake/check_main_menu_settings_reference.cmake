file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1d1355b5c5122a72da8298a72336d6dea707276f001e31844a149abcdbeba181")
    message(FATAL_ERROR "Expected the recorded original main-menu settings corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original main-menu settings comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
