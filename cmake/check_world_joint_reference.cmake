file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6a4b87bcd528c1e72b75f696f7f428c9e9d0c2f326dbe0d852d85007eb034bbe")
    message(FATAL_ERROR "Expected the recorded native world-joint graph corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native world-joint comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
