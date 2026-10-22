file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT reference_hash STREQUAL
    "dc10928fe849c8c78d87c3c70c9c69e8b60e6f3ace952099ce0d88544ba3e19d")
    message(FATAL_ERROR "Expected pinned APK and actual level behavior snapshots")
endif()
execute_process(COMMAND "${CHECKER}" --apk-trace "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics
    TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Level behavior comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
