file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2a7bbd14df6a7c90ca6d21638fe04166bb1b972705eff962742a778f84404f05")
    message(FATAL_ERROR "Expected the recorded original removal scoring corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal scoring comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
