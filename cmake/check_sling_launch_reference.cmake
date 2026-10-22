file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "04227533039879d178b8a3c495f19c3bb2ba0ea2e5b445d20e7ecf2850920a15")
    message(FATAL_ERROR "Expected the recorded original mutable sling launch corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Sling launch comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
