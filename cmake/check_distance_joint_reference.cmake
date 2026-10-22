file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7ee0c9409e2aa08ecae0f74642324c66542834cc4817ab570342fea3da43d0fb")
    message(FATAL_ERROR "Expected the recorded native distance-joint corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native distance-joint comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
