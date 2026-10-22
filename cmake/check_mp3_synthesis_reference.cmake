file(SHA256 "${REFERENCE}" reference_hash)
set(expected_hashes
    "e47295a78d0844e83063fe0febddbb4c21f19b535770c23fcc151366bd33ee96"
    "3197c976bd4b54ac8983a6442b651e5b6831df56a9e0d244894d10dce3b0d87d"
    "4317ccacbbc3ad50ff8cf0ac5ba965ad51eea9154fe1f6082c915f3032be585f")
list(FIND expected_hashes "${reference_hash}" hash_index)
if(hash_index LESS 0)
    message(FATAL_ERROR "Expected a recorded native synthesis corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native synthesis comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
