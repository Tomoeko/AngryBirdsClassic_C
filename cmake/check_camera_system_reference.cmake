file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned reference APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "30ea867a21775bc9dee1478ad11e1ffb4feed7e99a942356c4c9dd23762c3bb4")
    message(FATAL_ERROR "Expected the integrated original camera controller corpus")
endif()
execute_process(COMMAND "${CHECKER}" --apk "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera controller comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
