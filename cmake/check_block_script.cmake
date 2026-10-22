file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "074854b17ae49c7a735533312650d9841ba6f5f2d412101deef9fd5fd7bb8387")
    message(FATAL_ERROR "Expected the recorded original Android block-script corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native block-script comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
