include("${CMAKE_CURRENT_LIST_DIR}/check_level_order_inputs.cmake")
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2574c26df99fb53e397b8a9e57f3c83be5d1d22a8177dc999f722a5186905e44")
    message(FATAL_ERROR "Expected the recorded native 655-level constructor-order corpus")
endif()
execute_process(COMMAND "${CHECKER}" --levels "${DIRECTORY}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native level order comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
