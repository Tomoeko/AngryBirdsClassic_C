file(SHA256 "${APK}" apk_hash)
file(SHA256 "${BODIES}" bodies_hash)
if(NOT apk_hash STREQUAL
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad" OR
   NOT bodies_hash STREQUAL
    "3c1e6e44769d7f4c8870d0ff90703d01217a09ed7162bf3ff46960528af509de")
    message(FATAL_ERROR "Expected pinned APK and captured initial body corpus")
endif()
set(options --apk-bodies "${APK}" "${BODIES}")
if(STARTUP)
    file(SHA256 "${STARTUP}" startup_hash)
    if(NOT startup_hash STREQUAL
        "38b9084842faecaa39ba4e156e1af4c1e868a3a105bc26d286211d6958cfc2aa" AND
       NOT startup_hash STREQUAL
        "306874d46f0d74d18f173a390190b7ef33cb9099de64851d3d101e17b82ab520")
        message(FATAL_ERROR "Expected captured startup or post-timer character corpus")
    endif()
    set(options --apk-startup "${APK}" "${BODIES}" "${STARTUP}")
elseif(SPRITES)
    file(SHA256 "${SPRITES}" sprites_hash)
    if(NOT sprites_hash STREQUAL
        "1b8da696916cad7bedb3c6b5484ae2c12207943b14a119b2c45fb86766e00148")
        message(FATAL_ERROR "Expected captured initial sprite-name corpus")
    endif()
    set(options --apk-sprites "${APK}" "${BODIES}" "${SPRITES}")
elseif(FRAMES)
    file(SHA256 "${FRAMES}" frames_hash)
    if(NOT frames_hash STREQUAL
        "5655026c5480f320b33c428fa35f1e1df73bef064b31db2614cc91c67ad3c082")
        message(FATAL_ERROR "Expected captured initial frame corpus")
    endif()
    set(options --apk-frames "${APK}" "${BODIES}" "${FRAMES}")
    if(CAMERA)
        file(SHA256 "${CAMERA}" camera_hash)
        if(NOT camera_hash STREQUAL
            "37f881b093c0cf5d98162d92e2fed8fb280af51d42560e8d0438cd3786d74374" AND
           NOT camera_hash STREQUAL
            "05d2d4d61d65e3db90c7d1945661063453b6dd8cab9c611a0184ccbeb6cd992c" AND
           NOT camera_hash STREQUAL
            "2c443cbcb32393c4084bf05541605ff719caa9e6ea7c6c977a7f6a73374eb2fa" AND
           NOT camera_hash STREQUAL
            "b4a55563e44b51b40b990c38b9ded939403613dd3fa60d8aceecc9f3ec9cec2d" AND
           NOT camera_hash STREQUAL
            "bb91cb74ad0797a3f7d8cd69a96c98333390f35bae8a6ad39bf7b6df5f1ec262")
            message(FATAL_ERROR "Expected captured live first-level camera/world corpus")
        endif()
        set(options --apk-camera "${APK}" "${BODIES}" "${FRAMES}" "${CAMERA}")
    endif()
endif()
execute_process(COMMAND "${CHECKER}" ${options} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 60)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Direct APK level comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
