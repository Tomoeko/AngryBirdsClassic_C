file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "cdb7eae8e02c786f41ec089a5b8074493ad6abf1011103819fee2867d1fbfdd9")
    message(FATAL_ERROR "Expected the recorded native TEXT catalog/lookup corpus")
endif()
file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 APK")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" "${APK}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original TEXT comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
