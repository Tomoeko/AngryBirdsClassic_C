file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "09bd58339eca7edb5691a6db882cbed46bd2629f1f8a5aadd7cb419f249a68b2")
    message(FATAL_ERROR "Expected the recorded original native object removal corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object removal comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
