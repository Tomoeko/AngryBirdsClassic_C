cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL CHECKER AB_REFERENCE_GAME_MODE_CHUNKS)
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
    message(FATAL_ERROR "Cannot prepare original game-mode functions: ${output}${diagnostics}")
endif()
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-set-oracle.lua")
stage_original(original_function mode-set.lua
    "8f692f6fefb84a77f6cfbbf28584519b6b6d74b570585e869471d7256d6aae4d")
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-pause-oracle.lua")
stage_original(original_function mode-pause.lua
    "37b0648abecd33ca6b31f236a5025e7bea12047403975d9d199f11ed542b761f")
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-ingame-oracle.lua")
stage_original(original_function mode-ingame.lua
    "e556b399ed2c3bd76d83242cddb53db82bf380c7133bd6ebeeca46eda03671f8")
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-is-level-oracle.lua")
stage_original(original_function mode-is-level.lua
    "cf215f1be5ad91b10a515bafefaafaeb2f13b06658479b897cf94e7ca2576af5")
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-is-museum-oracle.lua")
stage_original(original_function mode-is-museum.lua
    "fb0239d9b5b40977036344aeb30c80897bf6f79b12e4947e7503839ce476c5d1")
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-museum-level-oracle.lua")
stage_original(original_function mode-museum-level.lua
    "0a12d20044191284c9489c18296e8984457f5958d96f86d63951ed2628d091b5")
set(original_function "${AB_REFERENCE_GAME_MODE_CHUNKS}/runtime-game-mode-pause-visible-oracle.lua")
stage_original(original_function mode-pause-visible.lua
    "56ff3247c6155760b56c9ccdf64afb360c7a0e48c29287c0c768a40ba66ea305")
set(AB_ANDROID_ORACLE_KIND game-mode)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_game_mode_reference.cmake")
