cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL CHECKER AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
set(adb "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}")

function(stage_original variable name expected_hash)
    get_filename_component(chunk "${${variable}}" REALPATH)
    cmake_path(IS_PREFIX private_directory "${chunk}" NORMALIZE private_chunk)
    file(SHA256 "${chunk}" actual)
    if(NOT private_chunk OR NOT actual STREQUAL expected_hash)
        message(FATAL_ERROR "Expected pinned private original ${name}")
    endif()
    execute_process(COMMAND ${adb} push "${chunk}" "/data/local/tmp/ab520/${name}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage ${name}: ${output}${diagnostics}")
    endif()
endfunction()

execute_process(COMMAND ${adb} shell mkdir -p /data/local/tmp/ab520
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare original reload geometry functions: ${output}${diagnostics}")
endif()
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-between-oracle.lua")
stage_original(original_function reload-between.lua
    "cc87dd1caf201ce612c29cbb1c2b6a3c7e865305bc3813650c4f626b28233da4")
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-to-oracle.lua")
stage_original(original_function reload-to.lua
    "1f6c7b0c189781c5fbdbb1d97959665fe65048c5b388e159a7564876da46cc86")
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-position-oracle.lua")
stage_original(original_function reload-position.lua
    "b34b38048b1a286d3953ca1b7697332d48c720fe78afc823b82db63d92a675f2")
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-distance-oracle.lua")
stage_original(original_function reload-distance.lua
    "045f5c072b12e6ead0e5c2b36d78ca0e898cdcab6c2c407a1ed1e20148807e9a")
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-subtract-oracle.lua")
stage_original(original_function reload-subtract.lua
    "68b5e2e429feb4515cc2acc530d4313f6ad36967f1973c12a88fea5180dbfce9")
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-length-oracle.lua")
stage_original(original_function reload-length.lua
    "c01e782dc2e95f0c7cd0493e65708b23c51631902c3b4f234ad388bc0c1d2798")
set(original_function "${AB_REFERENCE_RELOAD_GEOMETRY_CHUNKS}/runtime-reload-vlength-oracle.lua")
stage_original(original_function reload-vlength.lua
    "065376f1e48a6f21a8804451f3b42df73eff5531952fc575342aa9d7c3e32a19")
set(AB_ANDROID_ORACLE_KIND reload-geometry)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_reload_geometry_reference.cmake")
