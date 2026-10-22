file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5865917c5b527804562485bddfa8f90c3dc62e6ad304c359b1c40a21c3010e3f")
    message(FATAL_ERROR "Expected the recorded original camera end-sequence corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Camera end-sequence comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
