file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "877f4894fa36f6a3caa1b12c179a242fd0243b5235be6a9f64d0410a19df3c7a")
    message(FATAL_ERROR "Expected the recorded original AngerLevel composition corpus")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env TZ=UTC0 "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "AngerLevel comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
