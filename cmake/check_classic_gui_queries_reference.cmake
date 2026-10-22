file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "04916118cf76144b3da61a420581c944c4cf0ebb3a7ec441d3fc6db8194fd464")
    message(FATAL_ERROR "Expected the recorded original level-start/notification corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "GUI query comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
