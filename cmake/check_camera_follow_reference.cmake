file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b83c6ed2785f27408451e6ea426b306366ab776d1295522b617f4a3d504139f0")
    message(FATAL_ERROR "Expected the recorded original follow-camera graph corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Follow-camera comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
