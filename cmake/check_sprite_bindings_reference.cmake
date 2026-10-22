file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned reference APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "cf484838b9a01cc403368aaeddc0d8b4e52ee2a049c789588794ef4470a84de9")
    message(FATAL_ERROR "Expected the recorded 22 native sprite bindings")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native sprite binding comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
