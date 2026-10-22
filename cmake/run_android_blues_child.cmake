cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_BLUES_CHILD_ROOT APK
                 SHOT_REFERENCE CHECKER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
set(adb "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}")

function(stage_original relative name expected_hash)
    get_filename_component(chunk "${AB_REFERENCE_BLUES_CHILD_ROOT}/${relative}" REALPATH)
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
    message(FATAL_ERROR "Cannot prepare original Blues child graph: ${output}${diagnostics}")
endif()
stage_original(animation/class.lua bc-class.lua
    "3c35b0b9da93fe13f308c8a3e12dc47cc5fe18a51939b1509de171ee102e4c40")
stage_original(camera/behavior-shallow-copy.lua bc-copy.lua
    "0841db1bf9b290b4104e9f6e68900994f1e92c63c6087e8c9bfe17ae0e9b9812")
stage_original(camera/behavior-merge-recursive.lua bc-merge.lua
    "9ebf54eaa36a906487c6def574ab192f88fff215c230342f5d0701b4651625d7")
stage_original(camera/behavior-base.lua bc-base.lua
    "d6a7229d1eca0b313e383a404907aa9a4d63a4293fe9364ef24073cd83bbb207")
stage_original(camera/behavior-utils.lua bc-utils.lua
    "78b0997d6c8470884090bb7da92adeb93c28c1becfaa0ab9bfd1b3bdbfb5ce8f")
stage_original(input/MuseumBluesTrajectoryRenderer.lua bc-blue.lua
    "7b4d9dc047adbb97906237472d22504892a589fb7a01f2e5e3f36080ecc438c1")
stage_original(input/behavior-game-object.lua bc-game-object.lua
    "5a1bff4fc869f0b1cfbb703dd2f4892362e9f578bef41dd84d740afde45c7658")
stage_original(input/runtime-blue-child-oracle.lua bc-child.lua
    "758e2995a9bac6aa81e71adbe9a8689ed0c2f99077a6e0c8e522ff9b376926c7")
if(AB_ANDROID_BLUES_STAGE_ONLY)
    return()
endif()
set(AB_ANDROID_ORACLE_KIND blues-child)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_blues_child.cmake")
