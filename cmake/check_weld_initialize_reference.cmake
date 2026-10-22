file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1759e3da05ab37d4f642dcb8ff8f3dceaaddbe771c15d9e257c2a5b8ec6e087e")
    message(FATAL_ERROR "Expected the recorded native weld initialization corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native weld initialization comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
