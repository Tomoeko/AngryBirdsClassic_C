file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "aabb4a4b7dced2a2c517569e73517d1748008991948b011ebcc276817f66d0ac")
    message(FATAL_ERROR "Expected the recorded original Android WaterEffect corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native water configuration failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
