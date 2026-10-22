file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "95d9254717381c532610adb8c7e301f21142ef42de60199bcc6a28fed16d4267")
    message(FATAL_ERROR "Expected the recorded original UTC SessioningTimer corpus")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env TZ=UTC0 "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Session timer comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
