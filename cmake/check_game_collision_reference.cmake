if(KIND STREQUAL "rules")
    set(expected_hash
        "dbc3eb59f757a156cd4c9610d52c1cdcb707a6f99c13676042631dc9a97883f1")
else()
    set(expected_hash
        "b9b4e4a9554cc7c601b3bcfd12b4103a224b099e8f9258b9ec4faa154824dea8")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the pinned Android GameLua collision corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Game collision comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
