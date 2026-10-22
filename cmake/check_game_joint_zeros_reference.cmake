file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b5dd105381c90cbbea05fb8817a1bc9748e5c20a921bd0a9c8242c792189ce38")
    message(FATAL_ERROR "Expected the recorded native joint-constructor signed-zero corpus")
endif()
execute_process(COMMAND "${CHECKER}" --zeros "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native signed-zero joint comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
