file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d829c75d6867b739ab34f3dc89bb44ce2bd1b002a851adcde0943bfef6e9f3dc")
    message(FATAL_ERROR "Expected the recorded original string map GC corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original string map GC comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
