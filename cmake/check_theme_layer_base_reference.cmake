file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "65c910ffdae00412454c1effadb3816ececab78c5a5ad7ed36739d0fa81d3f74")
    message(FATAL_ERROR "Expected the recorded original theme base image corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme base image comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
