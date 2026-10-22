file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT reference_hash STREQUAL
    "18359a4fdd3910da1d10a7c4b5937b271b3b26946385b6421c6612d8308e4e78")
    message(FATAL_ERROR "Expected the pinned APK and complete native MP3 PCM hashes")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 120)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Audio bank comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
