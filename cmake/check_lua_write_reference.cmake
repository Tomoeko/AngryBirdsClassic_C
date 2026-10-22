cmake_minimum_required(VERSION 3.22)

file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ce3b7a57e1026e0f232075eb67aa95046409c7c7052d1fda9d767edd17b38ede")
    message(FATAL_ERROR "Expected the recorded original saved-table writer corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original save writer comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
