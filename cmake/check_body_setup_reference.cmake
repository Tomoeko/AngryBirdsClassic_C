file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "3f81717b30327503373491e0c4ae2c71e961fb19b9013f3054699b7f931466ba")
    message(FATAL_ERROR "Expected the recorded Android body-setup corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android body setup comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
