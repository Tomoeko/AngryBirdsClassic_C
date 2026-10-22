file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e5f3eab9a6f122968465eb4a94ab0558a04832eb24d6487a8a62000826be2541")
    message(FATAL_ERROR "Expected the recorded native joint lifecycle/world-step corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native joint lifecycle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
