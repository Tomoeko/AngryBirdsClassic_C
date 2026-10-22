file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Angry Birds 5.2.0 APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "bec6c2cbcc4a41d319db67cfaec51e890e8972c4fbb5452fd29ed416b7436160")
    message(FATAL_ERROR "Expected the native rectangle-image default shader choices")
endif()
execute_process(COMMAND "${CHECKER}" --rectangles "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native rectangle material comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
