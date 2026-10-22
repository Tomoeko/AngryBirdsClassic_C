file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "03b709140edfa2d8a28669552092e905abf3dd613a65c273b417bba0ba64faba")
    message(FATAL_ERROR "Expected the recorded original break/damage scoring corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original scoring comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
