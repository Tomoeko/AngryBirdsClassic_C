cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_AUDIO_RAMP_FUNCTIONS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
set(names audio music effects set-music set-effects)
set(hashes
    "7155e2bca674542783617176f250d4f5d0c71acbab0536eac75051d9e82a1288"
    "3296524647ddd7dcfd504b0d16281f48440c2ceccbd854ba680082a8872ea665"
    "90529049a79d0c0ce586465814e592c9148901b8db4ab42a06207306990c1489"
    "4f715f5c18082bade4d8de470a0e5e57232faf3dd7fd267c85e53696851030a0"
    "f326075efe6b576201a10775c2eb1e0101dae75db4b7b17acd7ae4b283346ecd")
set(adb "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}")
execute_process(COMMAND ${adb} shell mkdir -p /data/local/tmp/ab520/audio-ramp
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare original audio ramps: ${output}${diagnostics}")
endif()
foreach(index RANGE 0 4)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    get_filename_component(chunk
        "${AB_REFERENCE_AUDIO_RAMP_FUNCTIONS}/runtime-audio-ramp-${name}-oracle.lua" REALPATH)
    cmake_path(IS_PREFIX private_directory "${chunk}" NORMALIZE private_chunk)
    file(SHA256 "${chunk}" actual)
    if(NOT private_chunk OR NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected private original audio ramp ${name}")
    endif()
    execute_process(COMMAND ${adb} push "${chunk}"
        "/data/local/tmp/ab520/audio-ramp/${name}.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage original audio ramp ${name}: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND audio-ramp)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_audio_ramp_reference.cmake")
