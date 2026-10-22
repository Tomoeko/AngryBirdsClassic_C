cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_FRAME_TREE_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_FRAME_TREE_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original Frame hierarchy prototypes under Files")
endif()
set(names init add remove clear self pop enter exit layout child parent)
set(hashes
    "93a25a98711bfa48cbe663fe9f1cf98df6287b2b5fd223d6695346241f6ace45"
    "8424be3dbf793786d0337bbc8fb05302dd036bf0456ba35f874c21bae279cd50"
    "86fdfb05204be22b48360c8e569e78128524ef03371522ba8112b5d4ec57fd62"
    "b98d8688f3806867272e53179d931520856ba44717f5f89f0568cc86122651ad"
    "4fee84d10a2a879cec5fd008c5bfd5f8f46f9d50695fb1571480196a678bb278"
    "08055083add661433b3fa8d05308a76749c6ca8911af232be6889e960e5ff9e4"
    "dac1dac935c2b667828cbcba7897f7c19b69acff621c57f9dbe85cebf5429dff"
    "520720a06078095d0ab01ae22cace18fdbcb982bdc84402c30bf94f4e3577041"
    "3f8448fdf5d99559f87a46375b75093108249f1cbb9ea93187e4946043db54ae"
    "0dbb172aabb4ff29b9d0470131123fbb0d0c5c57a8779d37a1dac3fb63fcbad7"
    "19b1fea8aba93209efe411676b7936ec27f55524135b1c13d4762a7480a68283")
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare Frame hierarchy reference: ${output}${diagnostics}")
endif()
foreach(index RANGE 0 10)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    set(path "${chunks}/tree-${name}-oracle.lua")
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original Frame ${name} prototype")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${path}" "/data/local/tmp/ab520/ui/tree-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage Frame hierarchy prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND ui-frame-tree)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
