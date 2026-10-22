file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "96fb0c7e822380eb433b9b45d212683eaa90b04cd283af8ba5fb08cd4a31c9c0")
    message(FATAL_ERROR "Expected the recorded native synthesis-transform corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native synthesis-transform comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
