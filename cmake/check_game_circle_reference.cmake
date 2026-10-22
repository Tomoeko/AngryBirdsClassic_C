file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "fe15d7106e784192e3d5fbdcda78b3c813eaccbd1695b979b3f2c5e301afe663")
    message(FATAL_ERROR "Expected the recorded native GameObject circle corpus")
endif()
execute_process(COMMAND "${CHECKER}" --circle "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native circle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
