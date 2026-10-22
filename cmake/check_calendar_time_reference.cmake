file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "119c6edb27b01a250be43116586cd889ba6a2d05827381655a1c581abd008906")
    message(FATAL_ERROR "Expected the recorded original UTC calendar corpus")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env TZ=UTC0 "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Calendar comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
