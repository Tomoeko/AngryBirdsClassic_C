file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "781c044a21bd323a0db736cce27f21d8429fc1335f7b536a1c064fb46bbf38dc")
    message(FATAL_ERROR "Expected the recorded original camera/predictor/world corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Camera/predictor/world comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
