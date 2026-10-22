file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT reference_hash STREQUAL
    "f0868feff58c0dd876dd45b839703195644064100bdd8cb761b0f0387a131957")
    message(FATAL_ERROR "Expected the pinned APK and original material-sprite capture")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original material-sprite comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
