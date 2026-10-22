cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_LIMITS_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_LIMITS_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera limits bytecode under Files")
endif()
set(names
    initialize input input-zoom input-position project position zoom broken extents clamp projection squared norm)
set(hashes
    "22ffe8d30a27aa1e4a636dbd0b442f06f71f1dc96b09b605c06e9aac5e14f1ac"
    "af07897f95fe8f64529f25d1e39d76b9982ceade17305ecfe85aa5b5565c36f8"
    "a8206981e51d7107f0a5883f25bdbf519ad1b97bb6d6575e7173c6323314caa7"
    "03f34cc8b6ea34d33763ed6ec39a65c42702c5b7b62b92a130e31decf5db9b42"
    "1aeed0bc021315237dda7880f8a136dbfa48fb5726235b18716670b416e3e531"
    "2531430f10c8a3014b1aec2fb20ff29a55f9136ecc6b9cbe46b875aad049c414"
    "864c971e107641c25f0eb53b42e66a23983d50a69f511e1ea267adc66bde7f10"
    "b28ebd464edce66ac2190e54d2e9f2329874988864fdb56ce740574e1998dd56"
    "9684efdb008bb51b7d90cd6cb1b6236e015f96fbe8d0042e6a7edf8cbbefe6df"
    "774ef789a4d7d66fca8c92f04e56544422c358c57f911d691addee1769693df8"
    "a46eb6e0f23b8bfdb72c3a1f5f3f1b73ad2340f6cbde73f6991f6a7ca29f8d9b"
    "329c62fdaa3462ba60636bdb74b513584374ddd16e514a12dcf4d04d098a5989"
    "b680b6a6608118c868793616d251151f8230fb595ee2092d761314e27990c1a5")
foreach(index RANGE 0 12)
    list(GET names ${index} name)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${chunks}/limits-${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned original camera limits ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/camera
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera limits reference: ${output}${diagnostics}")
endif()
foreach(name IN LISTS names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/limits-${name}-oracle.lua"
            "/data/local/tmp/ab520/camera/limits-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera limits prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-limits)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
