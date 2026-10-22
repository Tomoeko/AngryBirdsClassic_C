file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 resource APK")
endif()
set(hashes
    4dce256c0533421ee4d1bf03bcf79bb1a648ae991847680cd02f20b17ec52b89
    4b29da8e88d212acef831779897b8e0a77bde50b421b8b1944b45f206ed07048
    0a24285951ccb0a931c7cf9fb58a6f3bd5bf4e577e12ec54e64fb9c98b57508a
    13c46f4fcc01cb5d541b15d15e803fe2801dfb8e7416166df93212a6624ed1fc
    989553f1ad1f5964c12aaef49b8ddbd10afcf988616c3c4adf1189e4b8bc45b7
    f7e93007c8241798cdb4073c209161688d65e8199e602da96a3943b31f0c8be3
    3528c9d9247703e2c80735d56e4a7cc031c827259834fa2c261bf0f5c028012b)
foreach(index RANGE 0 6)
    list(GET hashes ${index} expected)
    file(SHA256 "${REFERENCE}/webp-${index}.coefficients" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected original VP8 residual set ${index}")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" --apk "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 residual comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
