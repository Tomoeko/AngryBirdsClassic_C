file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2c1193d417bb2f848838b71b797186e67f49b3fa4379c426672d99b5a4007b43")
    message(FATAL_ERROR "Expected the pinned native resizeCircle corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Circle recreation differs: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
