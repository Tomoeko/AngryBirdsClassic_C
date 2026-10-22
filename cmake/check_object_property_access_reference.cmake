file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "001764c8dc8a41afc85a95bd3111f7939c33a6bda3d15bcd64b58be47a43c4c5")
    message(FATAL_ERROR "Expected the recorded original ObjectFunctions property corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Object property comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
