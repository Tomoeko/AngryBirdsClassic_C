file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned original 5.2.0 APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5a2dd60c30ecaa6babc4643691525a83e09836897e02e37ebe42a37fcb82fa22")
    message(FATAL_ERROR "Expected the recorded native post-removal traversal")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 120)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native post-removal comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
