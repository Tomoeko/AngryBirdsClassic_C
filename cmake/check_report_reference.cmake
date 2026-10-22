if(KIND STREQUAL "CONTACT_REPORT")
    set(expected_hash
        "6abfaca5386635bf04bb7106008a9532a65919a297011485db37bc33da64ecae")
elseif(KIND STREQUAL "WORLD_REPORT")
    set(expected_hash
        "82edddca089019421c027bbdcbfed46922f19bdfbbfcaf5664db244b1b1c0e14")
else()
    message(FATAL_ERROR "Select a recorded contact or world report corpus")
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded Android post-solve corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android post-solve comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
