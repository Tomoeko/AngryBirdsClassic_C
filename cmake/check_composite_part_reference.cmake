file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1af177e2074b16947e5cfa14207dd5baf002229172ed3264ad474301f575b6ba")
    message(FATAL_ERROR "Expected the recorded original composite part loader corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original composite part comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
