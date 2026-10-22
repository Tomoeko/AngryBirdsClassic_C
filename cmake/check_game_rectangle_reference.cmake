file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "22e88c22544e0dfce8747f17429f1078ceedcd5ce083ca3cfda03f4662934f18")
    message(FATAL_ERROR "Expected original GameLua rectangle fill arguments")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original game rectangle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
