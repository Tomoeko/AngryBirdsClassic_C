file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "75bdf15893a86c9e3586753831f6613ec55ac2c5d326f13369c0c69c87fdf95d")
    message(FATAL_ERROR "Expected the recorded native object-activity corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object-activity comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
