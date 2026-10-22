file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT reference_hash STREQUAL
    "f4a86c909ad3ceda4bd185d870d1510f5b2a69744fac4cf7e056b61f287c23e5")
    message(FATAL_ERROR "Expected pinned APK and native force-property snapshot")
endif()
execute_process(COMMAND "${CHECKER}" --apk "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native force-property comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
