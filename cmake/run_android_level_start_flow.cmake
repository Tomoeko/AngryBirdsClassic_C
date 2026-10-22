cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL CHECKER AB_REFERENCE_LEVEL_START_CLASS
        AB_REFERENCE_LEVEL_START_BASE AB_REFERENCE_LEVEL_START_MODULE
        AB_REFERENCE_LEVEL_START_EVENTS AB_REFERENCE_LEVEL_START_EXISTS
        AB_REFERENCE_LEVEL_START_PAUSING)
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
    message(FATAL_ERROR "Cannot prepare original level-start flow: ${output}${diagnostics}")
endif()
stage_original(AB_REFERENCE_LEVEL_START_CLASS flow-class.lua
    "3c35b0b9da93fe13f308c8a3e12dc47cc5fe18a51939b1509de171ee102e4c40")
stage_original(AB_REFERENCE_LEVEL_START_BASE flow-subsystems.lua
    "0ad4eb7c596c05994673df4852883ef7e16ecc54486430cd411b1764bb0ef474")
stage_original(AB_REFERENCE_LEVEL_START_MODULE flow-module.lua
    "538871ff74b3312c6160680641e3f5432ae9f8ce169472b690dd7caf88cb0b10")
stage_original(AB_REFERENCE_LEVEL_START_EVENTS flow-events.lua
    "231f3e4e93ec935a094b6bfb6d72d6b9a08e6ecfe126e1828a88086a936e3a00")
stage_original(AB_REFERENCE_LEVEL_START_EXISTS flow-exists.lua
    "6844e866c6cb277ef4374bc179d7cfbe294d80ace76eb3cff577c757739d3db0")
stage_original(AB_REFERENCE_LEVEL_START_PAUSING flow-pausing.lua
    "144fbd8484af18aba238bab602843e43960e5f328d76757beb221471378a68e5")
set(AB_ANDROID_ORACLE_KIND level-start-flow)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_level_flow_reference.cmake")
