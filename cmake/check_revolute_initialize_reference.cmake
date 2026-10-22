file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7a6af9016e77f5132c817d391d8c499f97138dbda46f87d1f502ab30e8196924")
    message(FATAL_ERROR "Expected the recorded native revolute initialization corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native revolute initialization comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
