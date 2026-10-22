file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2a727593292737e0dd9b7908c76622dd134a1c43e50fb8f0931ccf568c2d7881"
    AND NOT reference_hash STREQUAL
    "336a2a558cd7afc22f1bb84c079198d447784b29bd2b67d506feeeecc6251a47")
    message(FATAL_ERROR "Expected a recorded original owned level-camera corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Owned level-camera comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
