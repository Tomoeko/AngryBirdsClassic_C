file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 WebP image APK")
endif()
set(pixel_hashes
    "4453be45189d7c3d1c1742b057a5df67b4c1935e80973f795743c3239f78999e"
    "6c246837453e873348cb7ea2eb246204bfc490df78638496044e6ecbdd313f81"
    "39fd1405c00894c320323d4a63719bafb3656c153123f4ed808c3c7002ffd805"
    "9eff732ba4f20d64b4a8f0cd8371c917fe3f25066fd301d6cbaefecda3d8b49c"
    "5f9c1854309c57489c7c409025499ecb3b89c9c4e985e37e89a9fe276ea4be86"
    "81146d1715e1cdbd0134fe2ea4273e6c7c8a607180711892a352ee4cce186ab7"
    "daf4e6a56647059aebbf1819517857eba449c287e09fb2e9c1b807cdb31f6ee2")
foreach(index RANGE 0 6)
    file(SHA256 "${PIXELS}/webp-${index}.rgba" pixel_hash)
    list(GET pixel_hashes ${index} expected)
    if(NOT pixel_hash STREQUAL expected)
        message(FATAL_ERROR "Expected original WebP RGBA reference ${index}")
    endif()
endforeach()
set(mode --alpha)
if(DECODE)
    set(mode --apk)
endif()
if(COLOR)
    set(mode --planes)
    set(plane_hashes
        "0ca6f9a204d16fcefdbd4e950510c4b5efba9bf676f5a4d9eab5d74f5347a095"
        "bc02deefbdd9da713891d34fdc113c4b9e00a86ea0607a782572c93ab342dfd9"
        "4ca5867efea6ac9cfa85d6b9b0ca8d6a2c11884a6d13d698674fc80ffef35610"
        "ef431b659ab75d2a90048aca995838c4164cc3189976e3f379f209668155b04a"
        "9442140b705234b9d59a862984420240a957dbd9b4ab2701c59dcb8a193c1a51"
        "035e5537f081ffca73aa5d0841813168fd63caf4142baf835f67141c5f6bc226"
        "cdb436f3409a0ab85c1dda7d758344e4d15c9b42856a54b8262c1ee2ce891087")
    foreach(index RANGE 0 6)
        file(SHA256 "${PIXELS}/webp-${index}.yuv" plane_hash)
        list(GET plane_hashes ${index} expected)
        if(NOT plane_hash STREQUAL expected)
            message(FATAL_ERROR "Expected original WebP YUV reference ${index}")
        endif()
    endforeach()
endif()
execute_process(COMMAND "${CHECKER}" ${mode} "${APK}" "${PIXELS}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original WebP pixel comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
