file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d0dbeb147654568e7958f824c7637c4bd9021bc03efa5d22804658078c98d0a6")
    message(FATAL_ERROR "Expected the recorded native contact-handler routing corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native contact routing comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
