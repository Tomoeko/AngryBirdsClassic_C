file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned reference APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "001d281b617b609db1e9392982939ee0db00748ac7771f31d9590f6f558fce93" AND
   NOT reference_hash STREQUAL
    "3e79a61a6a84fe6acca729bb9f5f6088a2260c53b2ea34d3956c124095d5a98f")
    message(FATAL_ERROR "Expected the recorded original camera touch corpus")
endif()
execute_process(COMMAND "${CHECKER}" --apk "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera touch comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
