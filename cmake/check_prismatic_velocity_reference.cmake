file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7d92b0c1d77fef912973ecb5a35835e18cc5f39205a1d37f77b412d9efb3c37b")
    message(FATAL_ERROR "Expected the recorded native prismatic velocity corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native prismatic velocity comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
