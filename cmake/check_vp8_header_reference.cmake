file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 resource APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "565688517eda2bb79cc1560ed079e142a1dd4f0fbd4454bbcf2897f14e6455a1")
    message(FATAL_ERROR "Expected the original seven VP8 headers and arithmetic state")
endif()
execute_process(COMMAND "${CHECKER}" --apk "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 header comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
