file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6053d53f3593b87282098e022f1c6e36d1f64b7f2ec38bae909892c36004ac86")
    message(FATAL_ERROR "Expected the recorded native hybrid-window corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native hybrid-window comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
