file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
   "02e23f175c4617d127d6de83b6a6a6273d73b76c448af4e8d85d9d8057ec0bef")
    message(FATAL_ERROR "Expected the recorded complete MPEG-2 gain table")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native MPEG-2 gain comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
