file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b21ae7e83f44fbbed76fdd72a39f3417b02f571d23b704e49fb36290299f547d")
    message(FATAL_ERROR "Expected the recorded native image rectangle corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native image rectangle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
