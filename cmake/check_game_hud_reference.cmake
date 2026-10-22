file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "f394a46ccf01ec16fc772c9048fb14c06e00282acc6e607d5dfbcf408f57d331")
    message(FATAL_ERROR "Expected the pinned original GameHud draw trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native HUD comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
