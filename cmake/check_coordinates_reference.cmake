file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0b323ab2ab8c5c63c41cc4d011372319aa7af046d556df11ce501195b143a9b8")
    message(FATAL_ERROR "Expected the recorded native coordinate corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native coordinate comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
