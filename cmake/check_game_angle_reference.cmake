file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "cc72819ee1659e6344d7a37fc32423c6c1c0912bee236374ec3ce73a9a8acc4d")
    message(FATAL_ERROR "Expected the recorded Android game-angle corpus")
endif()
execute_process(COMMAND "${CHECKER}" --angles "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android game-angle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
