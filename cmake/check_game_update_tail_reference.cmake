file(SHA256 "${REFERENCE}" actual)
if(HOOKS)
    set(expected
        "806ec9cbdb88576d5f9750dafe81af9b534673ad0c9d0abbe5d8de26a7560094")
    set(check_arguments --hooks "${REFERENCE}")
else()
    set(expected
        "a259b8591964e574cd9990ea69f8e060693cd62b11c641fbddb9446c308f3ce6")
    set(check_arguments "${REFERENCE}")
endif()
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Expected the pinned original ordinary game tail trace")
endif()
execute_process(COMMAND "${CHECKER}" ${check_arguments}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native game tail comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
