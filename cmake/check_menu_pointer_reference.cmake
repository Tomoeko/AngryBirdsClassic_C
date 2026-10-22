file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d156b8c236e3502d06573b1988be3233abbc13bc1ddf398d61529957b15f149c")
    message(FATAL_ERROR "Expected the recorded original menu pointer corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original menu pointer comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
