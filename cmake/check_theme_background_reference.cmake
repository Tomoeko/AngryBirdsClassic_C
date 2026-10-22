file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "53e4db44f3964deedcd16c048136e60a91145a5a64e5ddb00155fbf5a52036d8")
    message(FATAL_ERROR "Expected original ThemeComposition background corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original background composition failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
