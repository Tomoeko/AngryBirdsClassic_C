file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "f485fdf57a0ffd8af1b179d717bda8abd0e39ccb21d44f9a5953689a99de1bd5")
    message(FATAL_ERROR "Expected the recorded original ProcessManager corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "ProcessManager comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
