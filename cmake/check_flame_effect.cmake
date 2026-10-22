file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "abb39292ea488ba5dc4f09005dd751109c88047eb8c63a1ad45474c2796d9ace")
    message(FATAL_ERROR "Expected the recorded original Android flame-update corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native flame comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
