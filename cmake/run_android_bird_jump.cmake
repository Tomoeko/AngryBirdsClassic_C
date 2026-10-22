cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_BIRD_JUMP_CHUNK)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunk "${AB_REFERENCE_BIRD_JUMP_CHUNK}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunk}" NORMALIZE private_chunk)
if(NOT private_chunk)
    message(FATAL_ERROR "Keep the original idle-jump reference under Files")
endif()
file(SHA256 "${chunk}" actual_hash)
if(NOT actual_hash STREQUAL
    "c94cad18a1fd289db4e4ed441600adbfc92987014dbb46b8dbb2aaed5036429f")
    message(FATAL_ERROR "Expected the recovered target object-animation prototype")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/jump RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare idle-jump oracle: ${output}${diagnostics}")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    push "${chunk}" /data/local/tmp/ab520/jump/bird-jump-oracle.lua
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot stage idle-jump prototype: ${output}${diagnostics}")
endif()
if(NOT DEFINED AB_ANDROID_ORACLE_KIND)
    set(AB_ANDROID_ORACLE_KIND bird-jump)
elseif(NOT AB_ANDROID_ORACLE_KIND STREQUAL "bird-jump" AND
       NOT AB_ANDROID_ORACLE_KIND STREQUAL "bird-jump-overshoot" AND
       NOT AB_ANDROID_ORACLE_KIND STREQUAL "bird-animation" AND
       NOT AB_ANDROID_ORACLE_KIND STREQUAL "goal-animation" AND
       NOT AB_ANDROID_ORACLE_KIND STREQUAL "character-animation")
    message(FATAL_ERROR "Select an idle-jump or character-animation oracle")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
