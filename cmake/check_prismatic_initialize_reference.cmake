file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ddfaa50187e03e3253a5ac10c0d0842b1503a4d39e6e58bcfbd9b9ba6fe058a9")
    message(FATAL_ERROR "Expected the recorded native prismatic initialization corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native prismatic initialization comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
