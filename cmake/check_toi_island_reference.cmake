file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "67b7079ac8e90c4c720a5baac9b7e169819f27482c73295f8a4e707a6d27629a")
    message(FATAL_ERROR "Expected the recorded Android continuous-island corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android continuous-island comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
