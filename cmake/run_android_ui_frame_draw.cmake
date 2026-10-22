cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_FRAME_DRAW_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_FRAME_DRAW_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original Frame draw bytecode under Files")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare Frame draw reference: ${output}${diagnostics}")
endif()
set(names draw tagged)
set(hashes
    "07074296808f587f741f29e9c30f44d8fe30dcf12ae26bc2513648e6a194223f"
    "f0ceffa0ffa13f236fb5243268f1dddd1ed282010c04e4d07abd17ed3139e38c"
)
foreach(index RANGE 0 1)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/frame-draw-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original Frame draw ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/frame-draw-${name}-oracle.lua"
            "/data/local/tmp/ab520/ui/frame-draw-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage Frame draw prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND ui-frame-draw)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
