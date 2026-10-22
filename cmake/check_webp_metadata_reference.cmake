file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 WebP image APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ec6e61c5e9783dbfb9b09fa46e90f82a38b0c7ddff4fb4376a8389a64a2273b5")
    message(FATAL_ERROR "Expected the recorded original WebP image metadata")
endif()
execute_process(COMMAND "${CHECKER}" --metadata "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original WebP metadata comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
