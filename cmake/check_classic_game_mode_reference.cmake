file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a3a58a65c9d7b702001ab0faf1a0be6489753ae158cd95e4bc6017153946e638")
    message(FATAL_ERROR "Expected the recorded original game-mode corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Game-mode comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
