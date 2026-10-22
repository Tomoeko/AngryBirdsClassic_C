file(SHA256 "${REFERENCE}" reference_hash)
file(SHA256 "${APK}" apk_hash)
if(NOT reference_hash STREQUAL
    "ee1707ce089da35c9252fd6d257950a2e044c7570e805c51fcdf64355c62dc17" OR
   NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the recorded Android asset-profile corpus and pinned APK")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" "${APK}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original asset-profile comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
