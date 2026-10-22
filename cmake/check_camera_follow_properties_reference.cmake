file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "58a4b482a6c18905a63d9013d915db012cf7dbf690aa9ac02a236713247588a6")
    message(FATAL_ERROR "Expected the recorded original camera/world/property corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Camera/world/property comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
