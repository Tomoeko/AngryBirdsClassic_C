file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e981944c67db00d5c4d8be438530cf7f0730de30a7abe4c805b4dd4c0f751590")
    message(FATAL_ERROR "Expected the recorded original Android object-transform corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object-transform comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
