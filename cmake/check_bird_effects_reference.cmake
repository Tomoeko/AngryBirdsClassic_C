file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1ad1f2e00309bb0b8acb3fd188351dc1790d87d616ffbbdfd6ab5b6c5d184771")
    message(FATAL_ERROR "Expected the recorded original bird contact-effect corpus")
endif()
if(COMPOSE)
    set(additional_arguments --callback)
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" ${additional_arguments}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird contact-effect comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
