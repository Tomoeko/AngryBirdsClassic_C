file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2ba343003901752f0152bc7b18cfe18c1a296f8ae223d41435b5b9466e6a409d")
    message(FATAL_ERROR "Expected the recorded original menu/frame keyboard corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original frame keyboard comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
