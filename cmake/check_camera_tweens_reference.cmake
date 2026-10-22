file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "41da3da0bbd9aee69432e9a23236ed76529de33d5ea0ef8a1abf2dacbef6387d" AND
    NOT reference_hash STREQUAL
    "3a14e398f8e1761d2337b61d98954c95f442ea95bb815c08f721afc3933fb50f")
    message(FATAL_ERROR "Expected the original camera mover/tween corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera mover/tween comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
