file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0ef5c42cefd4ec66a0a2a70f34fce9a1540558efae141a8344f20056b9396112")
    message(FATAL_ERROR "Expected the recorded original complete removal-pass corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Complete removal-pass comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
