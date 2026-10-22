file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d0193273d0cd3485bf2021b5513dc8197b74478b0c58b56b213126a9e790e00e")
    message(FATAL_ERROR "Expected the recorded original damage-particle corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Damage-particle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
