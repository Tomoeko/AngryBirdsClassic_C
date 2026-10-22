cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_TWEENS_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_TWEENS_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera tweens bytecode under Files")
endif()
set(names
    mover-reset
    mover-update
    mover-position
    mover-zoom
    mover-set-position
    mover-set-zoom
    mover-clear
    mover-cubic
    mover-set-tweens
    mover-cubic-out
    mover-end
    mover-cosine)
set(hashes
    "e6ad5a87aae3fcc8e5a3199aab826e53a34802571fc2a8cf0d804c013f6d4091"
    "9e3b2eef501e67f84855fdc3c3a87ce0a0e0b44628379320bbd4921e2322a891"
    "d274da4b123cb6ab6713fd0d52c1a37d4419d30cb20aee1127a369e63853939f"
    "3ff3d9ce5f2c63f291cc24f31f5e4e221f6187d9a75c3ef5056bca7be6803511"
    "661f7793c1526d0b49e605ef6a63faf434c9aee6fa71b54844174d42c0520238"
    "e123f708702631bf71424afe75378ca91be09b05c82496234348843bbbf4dd3b"
    "45845d311d97e2bbf8a87b523f03ddd59bf671459ec58327d1123a7ef39585dc"
    "ca9cc3844788cc79086bd9097d0d6b1be20f08b1d810594d84e3712255c8b336"
    "c4f9af856da10fb0a9a4858da80dffaef78cc641aa557944ab3255b14f059e9a"
    "e6cb261ec75c1f0de0b156b4a19b9136b10576201b12ff65b0a514cfef5f1ab5"
    "255b9a19269ce09d0f1c01348f27b19d54c64dfdcc72c72cb96cec301b1847d8"
    "b9354316df945250378bb4364a1b6798ba6786fb3470680a87722299f52d9aca")
foreach(index RANGE 0 11)
    list(GET names ${index} name)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${chunks}/${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned original camera tweens ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/camera
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera tweens reference: ${output}${diagnostics}")
endif()
foreach(name IN LISTS names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/${name}-oracle.lua"
            "/data/local/tmp/ab520/camera/${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera tweens prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-tweens)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
