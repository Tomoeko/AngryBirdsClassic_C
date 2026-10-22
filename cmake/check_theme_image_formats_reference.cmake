set(paths "${APK}" "${BINDINGS}" "${REFERENCE}")
set(hashes
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad"
    "513e712bcc3d5844f803d0632b49fe547361b3813dec61ebc937bad4bf933a41"
    "4ea79a3a4780a0d874ad753fe60a5cdb23b0ffc967209c5d62e679927df5f6a7")
foreach(index RANGE 0 2)
    list(GET paths ${index} path)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${path}" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned APK and live theme1 image evidence")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" "${APK}" "${BINDINGS}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Live theme image format comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
