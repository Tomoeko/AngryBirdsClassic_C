file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b1999cc42b3ba8be61c1b007329444cd895d38d71969f9b489210b4931b4abb2")
    message(FATAL_ERROR "Expected the recorded native short-transform corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native short-transform comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
