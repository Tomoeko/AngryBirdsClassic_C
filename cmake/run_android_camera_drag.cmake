cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_DRAG_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_DRAG_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera drag bytecode under Files")
endif()
set(names
    start update release dragging sample average)
set(hashes
    "eb24199d2a5a85fd275d20cdfb3e7435af4188609689a1d42eeeb88f2661aace"
    "e20522c42200220437dcaee86d327b057c0039009ea97d2be40aa83f5d2b0e2f"
    "05981bd7342b279f31e3ecea9e03e2d1bd98cc9ec24c45910838a34a9551210c"
    "8a0f202440dc1e577ddd86de006758c041c5e71d406332723aab821b70567ca8"
    "79174e4f9e7db3cdb523e1265e0fcf80c703f81bd937f310105b8b981e60fdcd"
    "585b9a1135f14a6778ae3571304aeba4ea9a8c7189025e378265f6ee8d6e9a20")
foreach(index RANGE 0 5)
    list(GET names ${index} name)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${chunks}/drag-${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned original camera drag ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/camera
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera drag reference: ${output}${diagnostics}")
endif()
foreach(name IN LISTS names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/drag-${name}-oracle.lua"
            "/data/local/tmp/ab520/camera/drag-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera drag prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-drag)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
