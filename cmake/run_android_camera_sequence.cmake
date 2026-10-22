cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_SEQUENCE_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_SEQUENCE_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera sequence bytecode under Files")
endif()
set(names
    reset start update next condition end)
set(hashes
    "850cf05dc8c913eb62279baf5e40b07a5388407af75e401752c1d2a00c70d015"
    "2a4b463fab7e47861f57d102147527ce278519dbc8df68d9dd61aed1503387aa"
    "19bfce35e3c5f6943b0da89bc54898241f68588e963092ad5d4627b5d25e1181"
    "bc07d32af4ca5f1f645c8c31d7b6de169d51aedf762d83bffd559c7178acdfa4"
    "8868656065ac82d756d5a3e70ab9a76defb381369d3a0a94e1c0e61ea9ab0583"
    "f6fac87fd5522e1e0c6e5edc45b2c1009cdb96d09eea59973bef242cf855c38c")
foreach(index RANGE 0 5)
    list(GET names ${index} name)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${chunks}/sequence-${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned original camera sequence ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/camera
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera sequence reference: ${output}${diagnostics}")
endif()
foreach(name IN LISTS names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/sequence-${name}-oracle.lua"
            "/data/local/tmp/ab520/camera/sequence-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera sequence prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-sequence)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
