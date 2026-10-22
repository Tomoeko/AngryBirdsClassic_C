cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_IMAGE_ANIMATION_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_IMAGE_ANIMATION_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original ImageButton animation bytecode under Files")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare ImageButton reference: ${output}${diagnostics}")
endif()
set(names update enter leave factor flags linear)
set(hashes
    "d3a0ce5b7e3983ea9af1e85cbfc4f9fce28abefc431999f7074938140329023e"
    "09f481cf5a7486cccf347da6432df9e45dd3bee0a2d1324fe08f7f24d69fb840"
    "3601a0a8f80e742f0a7a70dd0ad1c664bd9521328b80a0749f3702bf5af2001f"
    "414d99cde195bacf03916f91f24edf7db3e0aad082d676db2e75fd17f431710e"
    "db662a9c5d57b1877a0ccc54ad0f80e81c498d0fb3b91e434da344816c6ab460"
    "569a2d69ccaeafdaecf44b237e4b2942bb79eca63640af2913305c0457c23d1d"
)
foreach(index RANGE 0 5)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/image-animation-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original ImageButton ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/image-animation-${name}-oracle.lua"
            "/data/local/tmp/ab520/ui/image-animation-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage ImageButton prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND ui-image-animation)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
