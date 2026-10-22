file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "11c0e808ac52e9fbad759fae401e90b6de3d573139af17d1e195dcd76352f48f")
    message(FATAL_ERROR "Expected the recorded native direct matrix solve corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native matrix solve comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
