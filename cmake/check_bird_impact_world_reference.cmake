file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7ab8d73e4351cb029ffff41b079499282f8f9d6bb05757b4a06e3c85c262ad03")
    message(FATAL_ERROR "Expected the recorded original coupled impact prediction corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Coupled impact prediction failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
