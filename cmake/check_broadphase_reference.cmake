file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a0f1a07cc7c19d50d74b2975084f4c2d8761eecf5e3eefe262de14cef130559d")
    message(FATAL_ERROR "Expected the recorded Android broadphase corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android broadphase comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
