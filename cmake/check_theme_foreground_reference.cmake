file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a40917190be83fe65331a0ac719c98e8f12244d69d127e486c351262aecf7677")
    message(FATAL_ERROR "Expected original ThemeComposition foreground corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original foreground composition failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
