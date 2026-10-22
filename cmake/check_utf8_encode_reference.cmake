file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6a07f49a90cab14fbded6026d111db44affbd87be0e084d204f45906251caca9")
    message(FATAL_ERROR "Expected the recorded original UTF8 encoding corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original UTF8 encoding comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
