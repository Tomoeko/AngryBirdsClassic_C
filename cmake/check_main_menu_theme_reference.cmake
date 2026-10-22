file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b9ec3eede3635d8720a0688f47c19596fa40a1e26d98dac6f2b37ccb56cf5490")
    message(FATAL_ERROR "Expected the recorded original main-menu theme corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original main-menu theme comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
