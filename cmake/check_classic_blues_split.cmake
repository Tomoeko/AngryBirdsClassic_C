file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" native_hash)
file(SHA256 "${SHOT_REFERENCE}" shot_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT native_hash STREQUAL
    "eb24b41b3b98047cf54bfdffe4cd8d99fb95b573514be68be10bcbf270c29ed3" OR
   NOT shot_hash STREQUAL
    "eebe91cb537840a48484902ac677b2639b2ce5a700ec7e658a6563d10e9373b1")
    message(FATAL_ERROR "Expected pinned APK, original outer Blues and prepared shot corpora")
endif()
execute_process(
    COMMAND "${CHECKER}" "${APK}" "${SHOT_REFERENCE}" --blues-split "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original outer Blues comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
