file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" native_hash)
file(SHA256 "${SHOT_REFERENCE}" shot_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT native_hash STREQUAL
    "44aa24bcead206ce13df1e70de6da762339d8296542a600863614bf2a3e84da8" OR
   NOT shot_hash STREQUAL
    "eebe91cb537840a48484902ac677b2639b2ce5a700ec7e658a6563d10e9373b1")
    message(FATAL_ERROR "Expected the pinned APK, native source Blues child and prepared shot corpora")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${SHOT_REFERENCE}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Source Blues child comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
