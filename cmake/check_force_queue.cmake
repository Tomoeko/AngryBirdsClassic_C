if(POINT)
    set(expected_hash
        "9af755bd0c5cdca65825c1bf37e63ddac881ab7c30756d0772e52d4fe4acf31f")
    set(options --point)
else()
    set(expected_hash
        "1ede6ed6636267aeab57300f4c38c154ac83b97f392ab1ebbabc1e3c3b67a84c")
    set(options)
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded original Android force corpus")
endif()
execute_process(COMMAND "${CHECKER}" ${options} "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native force comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
