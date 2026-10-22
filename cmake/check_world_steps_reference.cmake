if(NOT DEFINED KIND)
    set(expected_hash
        "0e5917e668ba1fc878c405ec2f5a668ef04c9ced73ac673cc816a2619eb61ab5")
elseif(KIND STREQUAL "events")
    set(expected_hash
        "b6464ab84c5c09886d96e25be9a0ea0ad847529b2d296dd3af7da331a1fb6a80")
elseif(KIND STREQUAL "continuous")
    set(expected_hash
        "2d245f864354efa46d66974b90c6360010ea67b3ab3e74c10128e9fd92231c6f")
elseif(KIND STREQUAL "continuous-events")
    set(expected_hash
        "de2cf53c2bf7a2297993e194512886b400728e7052e41084887ef07efeb7ac1e")
elseif(KIND STREQUAL "rejected-impacts")
    set(expected_hash
        "4e2b2f777e6b14e5fb545ae9b2084d1f128b4f47a9911719c832baa463d530d9")
elseif(KIND STREQUAL "operations")
    set(expected_hash
        "19a5bcf04e01977bb6a5b4bdc90ccfdff86f2eb59138beed1fe2f1c13de1c800")
elseif(KIND STREQUAL "lifecycle")
    set(expected_hash
        "02eab3d0d45122791281c80108f8f15566c01f6e7896eac5f5a4166e7af48c6d")
else()
    message(FATAL_ERROR "Select consecutive world steps or events")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded Android consecutive-world corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android world-step comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
