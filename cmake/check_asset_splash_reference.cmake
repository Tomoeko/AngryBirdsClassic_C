file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b7a0db43139d6394064e15294c77f163c55a6b82882d8225477bf946e5896ed7")
    message(FATAL_ERROR "Expected the recorded original Android splash corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Android splash comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
