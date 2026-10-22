file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "17e224fa35cb7035ffc4366fe0d9d09ee02441ece70253be3f3d1176308f1ff6")
    message(FATAL_ERROR "Expected the recorded native GameObject convex-polygon corpus")
endif()
execute_process(COMMAND "${CHECKER}" --convex "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native convex-polygon comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
