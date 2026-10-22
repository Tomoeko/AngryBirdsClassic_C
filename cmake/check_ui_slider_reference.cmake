file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ab329b54f6978ec1073c48060d839eeda2aa4754a4300894dfbdda81cb7fda29")
    message(FATAL_ERROR "Expected the recorded original Slider update corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Slider comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
