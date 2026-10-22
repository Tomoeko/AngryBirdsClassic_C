file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "4a385b9e51828effe83ed4574ace1d5be5cb7475a9ef1441d41a5ee7422048d4")
    message(FATAL_ERROR "Expected the recorded ordinary Lua asset sprite corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Lua asset sprite comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
