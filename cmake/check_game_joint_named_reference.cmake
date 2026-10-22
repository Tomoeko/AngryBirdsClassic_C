file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "417cddf994b53d3a6ca34190b4209b9ec35ebb1b544d36ba9a99d278d60ba47f")
    message(FATAL_ERROR "Expected the recorded native named joint-removal corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native named joint-removal comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
