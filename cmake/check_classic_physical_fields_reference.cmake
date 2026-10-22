file(SHA256 "${APK}" apk_hash)
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   (NOT reference_hash STREQUAL
    "b75aaeb2a995893c4cdaebbb2b0321825eaa368d0ff0966c459d8b526b2236cf" AND
    NOT reference_hash STREQUAL
    "957ac810a673b4ce2d50363feb9c5a5e414c7bb00bf30df972427f565eb2d3bd"))
    message(FATAL_ERROR "Expected pinned APK and actual raw physical-field capture")
endif()
execute_process(COMMAND "${CHECKER}" --apk-fields "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Physical field comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
