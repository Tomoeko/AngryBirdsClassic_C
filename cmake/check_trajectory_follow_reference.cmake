file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "25faad36f1e3131b3ecac11ad622fe72fdc5105eb3e01f4b7fcbebda3086d48e")
    message(FATAL_ERROR "Expected the recorded original trajectory follow corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Trajectory follow comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
