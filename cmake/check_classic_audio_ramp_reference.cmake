file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "63313201a2a65b6301262d0886315f3ce18c69b323acfa4a751a491a4007e794")
    message(FATAL_ERROR "Expected the recorded original audio-ramp corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Audio-ramp comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
