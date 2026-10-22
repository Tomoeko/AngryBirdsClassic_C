file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a640e2bd50fb6e5a44fe1b105441cfe2a1238bb489ba399807b962631be64e1e")
    message(FATAL_ERROR "Expected the recorded Android free-island corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android integration comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
