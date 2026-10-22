file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d81c5154f16de8233c69f14c405f69dd002be54b413e8e75f5816d96b9cfb60a")
    message(FATAL_ERROR "Expected the recorded original Android flame-draw corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native flame draw comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
