file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e4b6af3481970ab8c5dd9f7e233f33b2ffa86274bb13b38e9efcb9367eca6d67")
    message(FATAL_ERROR "Expected the recorded Android fixture-proxy corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android fixture comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
