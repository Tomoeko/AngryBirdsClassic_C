file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0d8c94f1f0c661dcec91ea35632ea11c478033382f5bb6056cef40ceda988367")
    message(FATAL_ERROR "Expected the original VP8 inverse transform corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 inverse transform comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
