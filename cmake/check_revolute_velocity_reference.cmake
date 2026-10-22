file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "fb3dc2dbb3e1279f28aa3a3d128f60293c75476b06aaa5bf5252c636f68f8769")
    message(FATAL_ERROR "Expected the recorded native revolute velocity corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native revolute velocity comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
