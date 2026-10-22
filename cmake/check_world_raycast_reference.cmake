file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "49dd02cc4b7da08fee21e64c1be7e661c34708b253b03fb8289f11dd0792cfbd")
    message(FATAL_ERROR "Expected the recorded original world raycast corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "World raycast comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
