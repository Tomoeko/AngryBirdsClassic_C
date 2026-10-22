cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_COLLISION_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_COLLISION_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original collision-reference chunks under Files")
endif()
set(chunk_names class vector2 museum-collisions)
set(chunk_hashes
    "3c35b0b9da93fe13f308c8a3e12dc47cc5fe18a51939b1509de171ee102e4c40"
    "4594b6a6aa8bc65978758b8c916b551352a99a1b1f00e459e086d6b578a09813"
    "286320fc4899571a371aa32806a5017ef050e2151f5d7e0a8be7fe20c34c013e")
foreach(index RANGE 0 2)
    list(GET chunk_names ${index} name)
    list(GET chunk_hashes ${index} expected_hash)
    file(SHA256 "${chunks}/${name}.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the recovered target ${name} chunk")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/collision RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare collision oracle directory: ${output}${diagnostics}")
endif()
foreach(name IN LISTS chunk_names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/${name}.lua" "/data/local/tmp/ab520/collision/${name}.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage collision oracle chunk: ${output}${diagnostics}")
    endif()
endforeach()
if(NOT DEFINED AB_ANDROID_ORACLE_KIND)
    set(AB_ANDROID_ORACLE_KIND block-damage)
elseif(NOT AB_ANDROID_ORACLE_KIND STREQUAL "block-damage" AND
       NOT AB_ANDROID_ORACLE_KIND STREQUAL "bird-damage" AND
       NOT AB_ANDROID_ORACLE_KIND STREQUAL "bird-route")
    message(FATAL_ERROR "Select the original-Lua block or bird collision oracle")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
