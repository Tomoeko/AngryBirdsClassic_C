file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 APK load lists")
endif()
execute_process(COMMAND "${CHECKER}" --apk "${APK}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "APK load-list cache/dispatch failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
