file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6e73af387bbf9fbf007c206863501d5c66ac40c214480f558034b4d431492da0")
    message(FATAL_ERROR "Expected the recorded original removeBird corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird-removal comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
