file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "97bc423c717f6de1a61a6be30182a9963bc019ca2361a75d60c0dd05edd39ff5")
    message(FATAL_ERROR "Expected the recorded Android contact-island corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android contact-island comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
