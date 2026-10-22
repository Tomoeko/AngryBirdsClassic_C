cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL CHECKER AB_REFERENCE_SESSION_TIMER_MODULE
        AB_REFERENCE_SESSION_TIMER_CLASS AB_REFERENCE_SESSION_TIMER_COPY
        AB_REFERENCE_SESSION_TIMER_FORMAT AB_REFERENCE_ANGER_MODULE
        AB_REFERENCE_ANGER_EVENTS)
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
    message(FATAL_ERROR "Cannot prepare original timer: ${output}${diagnostics}")
endif()
stage_original(AB_REFERENCE_SESSION_TIMER_MODULE session-timer.lua
    "80c233365198c6b8ae5756ce28b3a088bc0fef843ea40297892bc899ef6a35a5")
stage_original(AB_REFERENCE_SESSION_TIMER_CLASS session-class.lua
    "3c35b0b9da93fe13f308c8a3e12dc47cc5fe18a51939b1509de171ee102e4c40")
stage_original(AB_REFERENCE_SESSION_TIMER_COPY session-copy.lua
    "2b5d10e72d16f566859c9e5260e53fe356510e6de205b86d4d01c133ccfe2943")
stage_original(AB_REFERENCE_SESSION_TIMER_FORMAT session-format.lua
    "e70fb17b7b82ada551e9ed38013951e6f09b2cfc2a110715669b7ec1d51e3b82")
stage_original(AB_REFERENCE_ANGER_MODULE anger-level.lua
    "1e2942409ef88fe3ed0389e3377d83ecbe3f7734b761b5ac6cac5118ee0ca288")
stage_original(AB_REFERENCE_ANGER_EVENTS anger-event-manager.lua
    "231f3e4e93ec935a094b6bfb6d72d6b9a08e6ecfe126e1828a88086a936e3a00")
set(AB_ANDROID_ORACLE_KIND anger)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_anger_reference.cmake")
