file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "911531d1b4098bb0bf30d1b387d5397396330330f96800fd85a43f129ec9d2d3")
    message(FATAL_ERROR "Expected the recorded original UTC calendar parts corpus")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env TZ=UTC0 "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Calendar parts comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
