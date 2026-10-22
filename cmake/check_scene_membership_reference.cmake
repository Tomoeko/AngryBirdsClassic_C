file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a68b625219ef28004768ccdb7fa12fa2a03b74fe57b26268c0681ddd7efb781b")
    message(FATAL_ERROR "Expected the recorded original scene membership corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Scene membership comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
