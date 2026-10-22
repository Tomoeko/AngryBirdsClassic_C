file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7fb1116846ce7a417215f8a171b06a8778e17d77d966c809d7f537b26e2cc32b")
    message(FATAL_ERROR "Expected the recorded original bird collision sprite corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird sprite comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
