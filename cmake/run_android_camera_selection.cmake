cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_SELECTION_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_SELECTION_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera selection bytecode under Files")
endif()
set(names
    closest
    direction
    snap
    position
    distance
    relative
    snap-zoom
    zoom
    zoom-limits
    vector-distance
    vector-length
    length
    subtract
    dot)
set(hashes
    "23ae788381c2905063db82c04dcfc8ba22db9d8d2d57838da1441fe5caf9a814"
    "5a683831bf79a87506aa23abde5ef861d25c93b29b07902c593db6c9837a7942"
    "8fae9728b775ece58bdf01bbe0c45807c124d07e92ab1f33dfd0aaf8fd81e813"
    "b34b38048b1a286d3953ca1b7697332d48c720fe78afc823b82db63d92a675f2"
    "1f6c7b0c189781c5fbdbb1d97959665fe65048c5b388e159a7564876da46cc86"
    "60bfb8fb56cc154ac4585ae32156f3e727475b5429693f645a8f8dc114cd0435"
    "9fa917d753013b64f05dd3bc79c069c94c8235928f94ac7be00ef2ba0c18f19c"
    "56dd7d28dd573d740f81a0af4df7d10f7fd5c4ab796705c4fe4908ee8d2ae8af"
    "11196b77778f19d00914c2652d375077108fd121c86aa4156fb1c8f08804ad7e"
    "045f5c072b12e6ead0e5c2b36d78ca0e898cdcab6c2c407a1ed1e20148807e9a"
    "c01e782dc2e95f0c7cd0493e65708b23c51631902c3b4f234ad388bc0c1d2798"
    "065376f1e48a6f21a8804451f3b42df73eff5531952fc575342aa9d7c3e32a19"
    "68b5e2e429feb4515cc2acc530d4313f6ad36967f1973c12a88fea5180dbfce9"
    "1ad6bf8b2cbc99ed162341b041a094cc56099d94bf2448188b6427a46cf82c35")
foreach(index RANGE 0 13)
    list(GET names ${index} name)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${chunks}/selection-${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned original camera selection ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/camera
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera selection reference: ${output}${diagnostics}")
endif()
foreach(name IN LISTS names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/selection-${name}-oracle.lua"
            "/data/local/tmp/ab520/camera/selection-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera selection prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-selection)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
