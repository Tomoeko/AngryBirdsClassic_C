cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_SYSTEM_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_SYSTEM_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera system bytecode under Files")
endif()
set(names
    system-lockCameraLimits
    system-unlockCameraLimits
    system-getScreenX
    system-getScreenY
    system-getScreenLeft
    system-getScreenTop
    system-getScreenExtentsForPositionAndZoom
    system-getScreenLeftForRendering
    system-getScreenTopForRendering
    system-getZoomScale
    system-gotoTargetPosition
    system-gotoSnapPoint
    system-updateCamera
    system-getSnapPoint
    system-setCameraInputEnabled
    system-getClosestValidCameraValues
    mover-reset
    mover-update
    mover-updatePosition
    mover-updateZoom
    mover-setTargetValues
    mover-getZoomScale
    mover-setTargetZoomScale
    mover-setTargetPosition
    mover-setTweenFunctions
    limits-initializeLimits
    limits-lock
    limits-unlock
    limits-enforceUserInputValues
    limits-enforceUserInputZoom
    limits-enforceUserInputPosition
    limits-projectWantedValuesDown
    limits-getClosestValidValues
    limits-checkPosition
    limits-checkZoom
    limits-checkForBrokenConstraints
    sequence-reset
    sequence-startSequence
    sequence-update
    sequence-moveToNextStage
    sequence-setCondition
    sequence-delegateSequenceAction
    sequence-endSequence
    automatic-baseInit
    automatic-setAllowCancel
    automatic-init
    automatic-update
    utils-tweenEaseCubicInOut
    utils-clamp
    utils-vectorProjection
    utils-vectorLengthSquared
    utils-vLengthsq
    default-source
    restart-source)
set(hashes
    "c2dc865a78fffe4804e3f12a736e317c707cc894adf14e94b813086383b3b5f9"
    "7b2207bf52cd313f6fc109a5a11e06970d5696d3e4326bdbb8afccfe63f01f8c"
    "87bf392f2499af56713d90151c1bdb972fad3df5035015f920be38ad64da8de5"
    "bc5fec385f032037409e9d58e842e8776ed2c8675d263a408c9bee6bd35d3802"
    "18c7dab6efa5cc9d57094110503db1ac41ebdedb75350eb90a0de00027a50f56"
    "c7bf8d1443ba2c748486e1880ddc1187c317730f9257f838e1eaf0414aa36964"
    "9684efdb008bb51b7d90cd6cb1b6236e015f96fbe8d0042e6a7edf8cbbefe6df"
    "fd3e9902b4f6d03393420c25a8759770f97f0d06bc5dc479109d6f404f61ed98"
    "ab1cc87bd454157d186512eac7cab47bca8d4adea1f3fd13a6bdf9dde9ae93fb"
    "56dd7d28dd573d740f81a0af4df7d10f7fd5c4ab796705c4fe4908ee8d2ae8af"
    "c5e264264fd3c2341d749fac04fc341b47c53aaee944b3a63dfa0d696bc9be7b"
    "18b643a7d87fed32207cfaf22b2d183ed2c4fea7174fa427483b9555844c4579"
    "e81622ae577e7a745bb0c3cc0bdadf2abb12fc8c7be91cf8e0573fcac1162e9b"
    "8fae9728b775ece58bdf01bbe0c45807c124d07e92ab1f33dfd0aaf8fd81e813"
    "58e198b4c435f098a61b0fb2adc1448fe32fa57e3e6badb9115eee39a878340d"
    "1b2df8a4394e85b23aa18db97ef5fb8efd6ed4516b6d224a9f2b3d4593507466"
    "e6ad5a87aae3fcc8e5a3199aab826e53a34802571fc2a8cf0d804c013f6d4091"
    "9e3b2eef501e67f84855fdc3c3a87ce0a0e0b44628379320bbd4921e2322a891"
    "d274da4b123cb6ab6713fd0d52c1a37d4419d30cb20aee1127a369e63853939f"
    "3ff3d9ce5f2c63f291cc24f31f5e4e221f6187d9a75c3ef5056bca7be6803511"
    "49f470df3e724e740716c90bd94b685e9e22c39fc3d0072667baca5c35459d35"
    "51f43e693e0adda5e9ac49b398fcc6edbd7d2deb014de71cfbc81dacb60e370e"
    "e123f708702631bf71424afe75378ca91be09b05c82496234348843bbbf4dd3b"
    "661f7793c1526d0b49e605ef6a63faf434c9aee6fa71b54844174d42c0520238"
    "c4f9af856da10fb0a9a4858da80dffaef78cc641aa557944ab3255b14f059e9a"
    "22ffe8d30a27aa1e4a636dbd0b442f06f71f1dc96b09b605c06e9aac5e14f1ac"
    "e95526771c1f82604ab945d63accb2d2a15ff29a572ae95e76798c8264654c6a"
    "cdeb0be1c4cd863a3ef484a1460e3099aa27953da5d1545d9652d70ca9c65cc9"
    "af07897f95fe8f64529f25d1e39d76b9982ceade17305ecfe85aa5b5565c36f8"
    "a8206981e51d7107f0a5883f25bdbf519ad1b97bb6d6575e7173c6323314caa7"
    "03f34cc8b6ea34d33763ed6ec39a65c42702c5b7b62b92a130e31decf5db9b42"
    "1aeed0bc021315237dda7880f8a136dbfa48fb5726235b18716670b416e3e531"
    "589b21412a1dcfc4d80c042bd74930e32e4a1a3fe79abed3cbe6dc2dbbee868b"
    "2531430f10c8a3014b1aec2fb20ff29a55f9136ecc6b9cbe46b875aad049c414"
    "864c971e107641c25f0eb53b42e66a23983d50a69f511e1ea267adc66bde7f10"
    "b28ebd464edce66ac2190e54d2e9f2329874988864fdb56ce740574e1998dd56"
    "850cf05dc8c913eb62279baf5e40b07a5388407af75e401752c1d2a00c70d015"
    "2a4b463fab7e47861f57d102147527ce278519dbc8df68d9dd61aed1503387aa"
    "19bfce35e3c5f6943b0da89bc54898241f68588e963092ad5d4627b5d25e1181"
    "bc07d32af4ca5f1f645c8c31d7b6de169d51aedf762d83bffd559c7178acdfa4"
    "8868656065ac82d756d5a3e70ab9a76defb381369d3a0a94e1c0e61ea9ab0583"
    "3cf87346fffb82b50e24409c9a9f9f2fea15a928a3bed5adcbf0ec54188d42cd"
    "f6fac87fd5522e1e0c6e5edc45b2c1009cdb96d09eea59973bef242cf855c38c"
    "1684c047b084cccec6148f3b1e75698515bd6c641a3a68929855af6cfb211bd1"
    "59311437d1995b63b9b7c1ecf83dfe84cb932e88b2fa85e4245b3603bd6e5deb"
    "ab6e724b2a6946868aa2b195773eb7b58739e42cfc24fe0d25f1751e2f732bc2"
    "67aa191a592115af301b44a10e2ab30ad6a6ae9419ce5848a57b88c8ad60328f"
    "ca9cc3844788cc79086bd9097d0d6b1be20f08b1d810594d84e3712255c8b336"
    "774ef789a4d7d66fca8c92f04e56544422c358c57f911d691addee1769693df8"
    "a46eb6e0f23b8bfdb72c3a1f5f3f1b73ad2340f6cbde73f6991f6a7ca29f8d9b"
    "329c62fdaa3462ba60636bdb74b513584374ddd16e514a12dcf4d04d098a5989"
    "b680b6a6608118c868793616d251151f8230fb595ee2092d761314e27990c1a5"
    "941aea9d101efabfe96174ae39ac1ddbcc35a3ad719bc443999f6076f75ac990"
    "c2fdf92a43d28fdb01c786e09d125ff9ff6204ee62896d24727458c6f6445f46")
foreach(index RANGE 0 53)
    list(GET names ${index} name)
    list(GET hashes ${index} expected_hash)
    file(SHA256 "${chunks}/system-${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the pinned original camera system ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/camera
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera system reference: ${output}${diagnostics}")
endif()
foreach(name IN LISTS names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/system-${name}-oracle.lua"
            "/data/local/tmp/ab520/camera/system-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera system prototype: ${output}${diagnostics}")
    endif()
endforeach()
if(NOT AB_CAMERA_SYSTEM_STAGE_ONLY)
    set(AB_ANDROID_ORACLE_KIND camera-system)
    include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
endif()
