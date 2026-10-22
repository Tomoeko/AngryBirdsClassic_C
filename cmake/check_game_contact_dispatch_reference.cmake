file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "590479d49b861c1ce31ff2eaa6268740a2dfb380c526ed1db2a49d0e3648940f")
    message(FATAL_ERROR "Expected the recorded native outer contact dispatch corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native contact dispatch comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
