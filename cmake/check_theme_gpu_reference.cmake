set(paths "${APK}" "${NATIVE}" "${PNG}" "${GLES2}" "${INPUT}")
set(hashes
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad"
    "e5cc62e5d28378f22ff1de57bf884d44175b42a1889f6ae195ddd5e9b46ad2b4"
    "cdbb520f75b6d27e1211c2b9ee41ffb430b45d1410ea0d0a30afbc94a613d0bd"
    "d9d8f529f63683c91d6a2d61b8e517f796c4d72892636584ba9d489ee86e7f68"
    "0dd702b176e10ee3c30aac6e7917f49231d35d0da35cae533c9a4bc67d19f18f")
set(options)
if(IDLE)
    list(REMOVE_AT hashes 4 3)
    list(APPEND hashes
        "db4eb82467e33995fc153911522f637453719058fbfd65d93c437500f47431c1"
        "f9d05d1f325a524560640be797bbcd1cbb0425a6cdb686c6f36dcef2eeb2ccc7")
    set(options --idle)
elseif(HUD)
    list(REMOVE_AT hashes 4 3)
    list(APPEND hashes
        "e97f7083c3584336cca2e0f4473407c47752292f29e81dfb0a864279c337f974"
        "5245406a570acbfd76f666e4a0a143aa406c2abbadb68c8dba02ecbb5aaf603e")
    set(options --hud)
elseif(SLING)
    list(REMOVE_AT hashes 4 3)
    list(APPEND hashes
        "8529248e69b4f692156b31fada1cd5aa185eb922e6c2b8f2b1fdd54f3dcbc0cc"
        "aca537e6360c9cffc027fd5c48c82102d2d9879e80519866226564440a1ec14f")
    set(options --sling)
endif()
foreach(index RANGE 0 4)
    list(GET paths ${index} path)
    list(GET hashes ${index} expected)
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected the pinned APK/native/GLES2 framebuffer evidence")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" ${options} "${NATIVE}" "${PNG}" "${GLES2}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native theme GPU comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
