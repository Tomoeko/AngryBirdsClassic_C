file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "470299949b3d728cb4ef2692428c6b536198acb81d9f6f2cff831fe5236a992a")
    message(FATAL_ERROR "Expected the recorded original bird hat/score prelude corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird-prelude comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
