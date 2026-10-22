file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2accafbe267fb2a301e8cc9530e5d348ac88ca8de67d99396f78b3e3ea75da4b")
    message(FATAL_ERROR "Expected the recorded original camera smoothstep corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Camera smoothstep comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
