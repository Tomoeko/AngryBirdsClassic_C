cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_IMAGE_BUTTON_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_IMAGE_BUTTON_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original ImageButton state bytecode under Files")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare ImageButton reference: ${output}${diagnostics}")
endif()
set(names handle pointer flags events frame)
set(hashes
    "3c271012b1a48bbc4d76a582d2628c08fe39faa3d0df6f6c1d61f36ba3aab967"
    "4cc859d120f5d96499647b4273ef8ff395312cf69e6299100da7ab1d9a60d775"
    "db662a9c5d57b1877a0ccc54ad0f80e81c498d0fb3b91e434da344816c6ab460"
    "1a233ad1d8e3b34d268849446038b04d955f0309997282d4fef741524aa180df"
    "61495274776bee6843a16b26570fc8edf4a1bf31471186130a3cbe4dc9c4793b"
)
foreach(index RANGE 0 4)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/image-button-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original ImageButton ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/image-button-${name}-oracle.lua"
            "/data/local/tmp/ab520/ui/image-button-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage ImageButton prototype: ${output}${diagnostics}")
    endif()
endforeach()
if(AB_UI_BUTTON_STATE)
    set(AB_ANDROID_ORACLE_KIND ui-button-state)
else()
    set(AB_ANDROID_ORACLE_KIND ui-image-button)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
