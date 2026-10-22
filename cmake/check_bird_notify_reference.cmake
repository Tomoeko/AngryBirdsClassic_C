file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "778fb25658ed116adf025a414bb28d6c55c4f55ce333fcbdf4a2b082190326b3")
    message(FATAL_ERROR "Expected the recorded original bird-notification prefix corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird-notification comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
