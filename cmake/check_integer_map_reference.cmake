file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e5f207cd48137ec66174882d53a0512255b7db468ec917ff1640cd5c4d82d13d")
    message(FATAL_ERROR "Expected the pinned native integer mutation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native integer mutation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
