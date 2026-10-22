file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1433f10b4a781ceb32ceec78cf6435c0d103e25bc25777d414866eb4630bf200")
    message(FATAL_ERROR "Expected the recorded original GameObject behavior corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Game behavior comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
