file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT reference_hash STREQUAL
    "9aaecf3d9e87e8670e2de1184893919900311e4da45ea2730e4d01a18ddd2e4b")
    message(FATAL_ERROR "Expected the pinned APK and independent WAV reference manifest")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "PCM16 WAV comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
