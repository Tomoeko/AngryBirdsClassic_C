cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_MENU_POINTER_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_MENU_POINTER_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original menu pointer bytecode under Files")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/input
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare menu pointer reference: ${output}${diagnostics}")
endif()
set(names
    reset
    pointer
    clicks
    copy
)
set(hashes
    "051c768ee143b9fd719af6ae18827007bfbf7a7532ca195f30aa6bdaef3081a7"
    "d8fc98171a3cd6f85e76b7bf52140cb666f82dda7d5ea0b17d3db36a317f163f"
    "39893e0e533930b6ac2ff2c9014fb2a04a17de529cc1245ca1d4af015560ff75"
    "2b5d10e72d16f566859c9e5260e53fe356510e6de205b86d4d01c133ccfe2943"
)
foreach(index RANGE 0 3)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/pointer-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original menu pointer ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/pointer-${name}-oracle.lua"
            "/data/local/tmp/ab520/input/pointer-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage menu pointer prototype: ${output}${diagnostics}")
    endif()
endforeach()
if(NOT AB_MENU_POINTER_STAGE_ONLY)
    set(AB_ANDROID_ORACLE_KIND menu-pointer)
    include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
endif()
