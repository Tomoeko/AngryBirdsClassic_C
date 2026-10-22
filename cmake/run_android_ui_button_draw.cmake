cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_BUTTON_DRAW_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_BUTTON_DRAW_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original button draw bytecode under Files")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare button draw reference: ${output}${diagnostics}")
endif()
set(names self draw factor linear image frame tagged)
set(hashes
    "2f4a2c78fad7a2ac88fcf8ec9fb5bc6a83e55d38edbaf14255580c1909e34dc8"
    "a886864a9329ada2f2c038d3274052dc58fc75462d44ce8832408d2381f49ed2"
    "414d99cde195bacf03916f91f24edf7db3e0aad082d676db2e75fd17f431710e"
    "569a2d69ccaeafdaecf44b237e4b2942bb79eca63640af2913305c0457c23d1d"
    "73010b2c531fa383fe7130ed105832e965ad15858244fbd418a0a7c181db8c63"
    "07074296808f587f741f29e9c30f44d8fe30dcf12ae26bc2513648e6a194223f"
    "f0ceffa0ffa13f236fb5243268f1dddd1ed282010c04e4d07abd17ed3139e38c"
)
foreach(index RANGE 0 6)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/button-draw-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original button draw ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/button-draw-${name}-oracle.lua"
            "/data/local/tmp/ab520/ui/button-draw-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage button draw prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND ui-button-draw)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
