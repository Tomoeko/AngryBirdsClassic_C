file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "c0aad115f96011a7ad5c6e032fb28a0e0d3abd01f813dca1c22e1b67814bac27")
    message(FATAL_ERROR "Expected the recorded original theme transform corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme transform comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
