cmake_minimum_required(VERSION 3.22)
set(AB_CAMERA_SYSTEM_STAGE_ONLY TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_camera_system.cmake")
set(names
    touch-system-getScreenPosition
    touch-system-getClosestCameraSnapPoint
    touch-system-getSnapPointInDirection
    touch-system-getDistanceToSnapPoint
    touch-system-getRelativeZoomLevel
    touch-system-snapZoomLevel
    touch-system-toggleBetweenCloseAndFarZoom
    touch-system-init
    touch-system-isInputEnabled
    touch-system-setInputEnabled
    touch-limits-getZoomLevelLimits
    touch-utils-length
    touch-utils-vectorLength
    touch-utils-vectorDistance
    touch-utils-vLength
    touch-utils-vectorSubtract
    touch-utils-vectorDot
    touch-utils-tweenEaseCubicOut
    touch-utils-easeCosineInOut
    touch-FiniteStateMachine
    touch-DragHandler
    touch-TouchCameraInputHandler
)
set(hashes
    "b34b38048b1a286d3953ca1b7697332d48c720fe78afc823b82db63d92a675f2"
    "23ae788381c2905063db82c04dcfc8ba22db9d8d2d57838da1441fe5caf9a814"
    "5a683831bf79a87506aa23abde5ef861d25c93b29b07902c593db6c9837a7942"
    "1f6c7b0c189781c5fbdbb1d97959665fe65048c5b388e159a7564876da46cc86"
    "60bfb8fb56cc154ac4585ae32156f3e727475b5429693f645a8f8dc114cd0435"
    "9fa917d753013b64f05dd3bc79c069c94c8235928f94ac7be00ef2ba0c18f19c"
    "692b2d3cd4768de42582280ecd00c1d648f9562ede93f60673223acc4c79fc73"
    "7ba09dd7e6f26827d8cbdd8f2b46cdf53f78f68bf95b6e2eeec7305e069692f9"
    "80bfe61f73197ad043615ace6c3ccd021e74eb7e1150e3b7c0bfca358402ea3d"
    "ad1bde5a9a6372ebcc96fa85b35439489dd1c73ec9d0e387785d578f98bc78c1"
    "11196b77778f19d00914c2652d375077108fd121c86aa4156fb1c8f08804ad7e"
    "a0e9c19f091e507e421e93126c5458323decac39d602ccaccebd26ca7fc4fcfd"
    "c01e782dc2e95f0c7cd0493e65708b23c51631902c3b4f234ad388bc0c1d2798"
    "045f5c072b12e6ead0e5c2b36d78ca0e898cdcab6c2c407a1ed1e20148807e9a"
    "065376f1e48a6f21a8804451f3b42df73eff5531952fc575342aa9d7c3e32a19"
    "68b5e2e429feb4515cc2acc530d4313f6ad36967f1973c12a88fea5180dbfce9"
    "1ad6bf8b2cbc99ed162341b041a094cc56099d94bf2448188b6427a46cf82c35"
    "e6cb261ec75c1f0de0b156b4a19b9136b10576201b12ff65b0a514cfef5f1ab5"
    "b9354316df945250378bb4364a1b6798ba6786fb3470680a87722299f52d9aca"
    "19d551b98aa6f7f09331b473e523b7ce0746077c8184b5804926d61cb3557bbc"
    "6772846861bc68b40022d0ce72629caf8d9894f70fe0b7258c46873e3c470874"
    "c8d494f8e40629ab9eb07ad1c4d1e06341220fdf9421d941711b320e7c0466d3"
)
foreach(index RANGE 0 21)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original touch prototype ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/${name}-oracle.lua" "/data/local/tmp/ab520/camera/${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage touch prototype: ${output}${diagnostics}")
    endif()
endforeach()
if(NOT AB_CAMERA_TOUCH_STAGE_ONLY)
    set(AB_ANDROID_ORACLE_KIND camera-touch)
    include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
endif()
