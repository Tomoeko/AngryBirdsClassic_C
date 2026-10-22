file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "202544e38282c29733196c9092864cf08bbe6ce4e12b9b1ba74fe1f650c35505")
    message(FATAL_ERROR "Expected the recorded original menu frame navigation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original menu navigation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
