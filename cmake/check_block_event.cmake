if(PHYSICS)
    set(expected_hash
        "8a2544f870141fb5f4d4d9413e417d5822615f405c72483e3396cb50aa7ac14e")
    set(options --physics)
else()
    set(expected_hash
        "383f288cf3c493777a3150037a93d3a581fd3787dccdb3047f950f0eecffc9e9")
    set(options)
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded original Android block-event corpus")
endif()
execute_process(COMMAND "${CHECKER}" ${options} "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native block event comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
