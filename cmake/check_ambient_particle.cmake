file(SHA256 "${REFERENCE}" reference_hash)
if(COORDINATES)
    set(expected_hash
        "740dd3f9c9eaa3124bf6bd1ab6bf50b5e54832e1f021af44f0a7c461c5eb9e71")
    set(check_arguments --coordinates "${REFERENCE}")
else()
    set(expected_hash
        "2bbf13f8511b83db88af14d937dac540dd26943357811da1428faf8e63d157b3")
    set(check_arguments "${REFERENCE}")
endif()
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded original Android ambient-particle corpus")
endif()
execute_process(COMMAND "${CHECKER}" ${check_arguments}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native ambient-particle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
