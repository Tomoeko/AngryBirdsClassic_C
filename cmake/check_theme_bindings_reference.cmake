file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Angry Birds 5.2.0 APK")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "513e712bcc3d5844f803d0632b49fe547361b3813dec61ebc937bad4bf933a41")
    message(FATAL_ERROR "Expected the selected live theme1 sprite bindings")
endif()
execute_process(COMMAND "${CHECKER}" "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Live APK theme binding comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
