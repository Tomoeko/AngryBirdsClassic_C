file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "3089b4fe02140b237e69a7681e80e788086ccd87e71bdca3532646979fc7cd0b")
    message(FATAL_ERROR "Expected the recorded Android world-solver corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android world comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
