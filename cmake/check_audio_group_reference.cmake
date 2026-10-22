file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "64346db11c4ac39001a420a687895bd8ec4185d980f5e64ed68133449d540f56")
    message(FATAL_ERROR "Expected the recorded original audio-name choice corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Audio-name selection comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
