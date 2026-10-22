file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "c93140f6942b4e8f9b7f401703de18d75e3ca18ef73a8e59035c3379d8e49e6f")
    message(FATAL_ERROR "Expected the recorded native long-transform corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native long-transform comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
