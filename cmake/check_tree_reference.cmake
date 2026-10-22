file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "04308184ecfd80baccbaabf755025f91d3284582dcef296b5ff0e54127c32d32")
    message(FATAL_ERROR "Expected the recorded Android dynamic-tree corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android dynamic-tree comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
