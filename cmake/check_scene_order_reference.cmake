file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a08c1eb8d41150a6cd80afef5ed43c75a8109fba53a3006977f23f418191fc08")
    message(FATAL_ERROR "Expected the retained native scene order capture")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native scene order comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
