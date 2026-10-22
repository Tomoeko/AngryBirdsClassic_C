file(SHA256 "${APK}" apk_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")
    message(FATAL_ERROR "Expected the pinned Android 5.2.0 resource APK")
endif()
set(mode --apk)
set(suffix yuv)
set(hashes
    0ca6f9a204d16fcefdbd4e950510c4b5efba9bf676f5a4d9eab5d74f5347a095
    bc02deefbdd9da713891d34fdc113c4b9e00a86ea0607a782572c93ab342dfd9
    4ca5867efea6ac9cfa85d6b9b0ca8d6a2c11884a6d13d698674fc80ffef35610
    ef431b659ab75d2a90048aca995838c4164cc3189976e3f379f209668155b04a
    9442140b705234b9d59a862984420240a957dbd9b4ab2701c59dcb8a193c1a51
    035e5537f081ffca73aa5d0841813168fd63caf4142baf835f67141c5f6bc226
    cdb436f3409a0ab85c1dda7d758344e4d15c9b42856a54b8262c1ee2ce891087)
if(BYPASS)
    set(mode --unfiltered)
    set(suffix unfiltered.yuv)
    set(hashes
        321fb4d6871d5c216467565a765b29e09d2670f8f56a38e20c3dd03f77b510f2
        e4a06a04b25660d88116e752536324d1b0799aa83cf02303fe87d9016ee99f91
        2821c17d0bfed82a0c8213b4751ebc4de526706a7011cc900c2e95fb54632d0c
        a9cd0bbf2791a95f368fc702ae4ccf0d80dc840056c1fbc453b578547814f72f
        183e2aed9a3a2d6c765c794d833ac8f1b8740507f1428154501d1a5191236001
        47a245afa115a55a12a8035b664523049533bb6a98c04e95e4218c48bb524502
        b552d7045aa42374990266d0c17fd1c8e9062e9809b16aa026245ad4c779798f)
endif()
foreach(index RANGE 0 6)
    list(GET hashes ${index} expected)
    file(SHA256 "${REFERENCE}/webp-${index}.${suffix}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected original VP8 ${suffix} image ${index}")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" ${mode} "${APK}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 image comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
