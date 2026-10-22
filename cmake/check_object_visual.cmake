file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "064f819ea32009d34ea1279b9d5a0ee60bf8048fea6de24f7332cc074cecd0c8")
    message(FATAL_ERROR "Expected the recorded original Android object-visual corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object-visual comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
