file(SHA256 "${LIBRARY}" library_hash)
if(NOT library_hash STREQUAL
    "ada9bc1a1469c30a9bf906c4293df3ff1a75485b40ddbfb146debbc41f14d38c")
    message(FATAL_ERROR "ARM probe requires the pinned ARMv7 library hash")
endif()

set(math_loader)
if(MATH_TABLE)
    file(SHA256 "${MATH_TABLE}" math_hash)
    if(NOT math_hash STREQUAL
        "ba0eefe62768de82ed2fa830166f2426b64e03f474b31b88853da24efb49b03d")
        message(FATAL_ERROR "Expected the recorded Android 5.1.1 ARM math corpus")
    endif()
    list(APPEND math_loader -device "loader,file=${MATH_TABLE},addr=0x43000000,force-raw=on")
endif()

execute_process(COMMAND "${QEMU}" -M virt -cpu cortex-a15 -m 128M
    -nographic -monitor none -serial none -nic none
    -semihosting-config enable=on,target=native
    -d guest_errors -D "${TRACE}.guest-errors"
    -kernel "${PROBE}"
    -device "loader,file=${LIBRARY},addr=0x42000000,force-raw=on"
    ${math_loader}
    RESULT_VARIABLE probe_result ERROR_FILE "${TRACE}"
    OUTPUT_VARIABLE probe_output TIMEOUT 20)
if(NOT probe_result STREQUAL "0")
    message(FATAL_ERROR "ARM probe failed: ${probe_result}; inspect ${TRACE}")
endif()
file(SIZE "${TRACE}.guest-errors" guest_error_bytes)
if(guest_error_bytes GREATER 0)
    message(FATAL_ERROR "ARM probe accessed unsupported guest memory; inspect ${TRACE}.guest-errors")
endif()
execute_process(COMMAND "${CHECKER}" "${TRACE}"
    RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error TIMEOUT 5)
if(NOT check_result STREQUAL "0")
    message(FATAL_ERROR "ARM comparison failed: ${check_error}")
endif()
message(STATUS "${check_output}")
