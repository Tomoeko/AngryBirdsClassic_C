file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0f0828fa7e7e6dfd9eb725b7e84ac295ac7361f9b99bdf72250df23085b51510")
    message(FATAL_ERROR "Expected the recorded original mighty-bird collision corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Mighty-bird comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
