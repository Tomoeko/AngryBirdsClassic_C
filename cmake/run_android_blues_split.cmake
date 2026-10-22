cmake_minimum_required(VERSION 3.22)

# Reuse the pinned real native-circle/Class/GameObject input graph.
set(AB_ANDROID_BLUES_STAGE_ONLY TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_blues_child.cmake")
unset(AB_ANDROID_BLUES_STAGE_ONLY)

stage_original(input/runtime-blue-outer-oracle.lua bs-outer.lua
    "c9454c78115f051520cc385ce9f6db28ec5ec86a16021e21a0b04b1258ae8958")
stage_original(input/runtime-vNormalize-oracle.lua bs-normalize.lua
    "6d10efcda50f958034d8c6e6a69bba180a184856ba783256274dcd60bdba1f4b")
stage_original(input/runtime-bs-puff.lua bs-puff.lua
    "b20e7fe246565e2d866d4a51f06546494a93a5533057f6d72fd1584df8a621f8")
stage_original(input/runtime-bs-getTutorials.lua bs-get-tutorials.lua
    "791eea556c6a73f0d3fde2216dc78ea46d71dc6aafb1b890b684d1c15258894e")
stage_original(input/runtime-bs-getTutorialsForItem.lua bs-get-item.lua
    "40c1d3e00c8cb17f3b687eee449d137c78ae51256cee6af65f9e242e59bdf5a0")
stage_original(input/runtime-bs-setExtraTutorialShown.lua bs-tutorial.lua
    "0640c00895494a412984e79ca58e5e68bfda78b6d7d5e23fdf1184c1b8a0ea8a")
stage_original(camera/MuseumBluesCameraFollowBehavior.lua bs-camera.lua
    "848cb0b7222c6b909866ec39970d9c9bcdc37a1e86363c351b1cb27a6cd45f58")
stage_original(camera/trajectory-renderer.lua bs-trajectories.lua
    "5aa976a13a617e010a8614a59427903893467241c9a8dbdbde6fad3c774ad774")

set(AB_ANDROID_ORACLE_KIND blues-split)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
if(AB_ANDROID_BLUES_CAPTURE_ONLY)
    return()
endif()
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_classic_blues_split.cmake")
