file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "65bddddef48fb02d8febf2341e0365abdb14f007647fefb84a65c5ecebacfdf2")
    message(FATAL_ERROR "Expected the original game tail joint-phase corpus")
endif()
execute_process(COMMAND "${CHECKER}" --joints "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original joint-phase comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
