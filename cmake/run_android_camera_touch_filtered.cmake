cmake_minimum_required(VERSION 3.22)
set(AB_CAMERA_TOUCH_STAGE_ONLY TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_camera_touch.cmake")
set(AB_MENU_POINTER_STAGE_ONLY TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_menu_pointer.cmake")
set(names get-input get-touches)
set(hashes
    "4f0c44e4c89aedf5b4826ee6a42baa8e5cfff5b5d2b54280df2cbe7907fa6fa7"
    "b022bf9edea0b1d0032b0606a19e5b94968ca00214af868604e07785d9258cc6"
)
foreach(index RANGE 0 1)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/pointer-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original filtered input ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/pointer-${name}-oracle.lua"
            "/data/local/tmp/ab520/input/pointer-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage filtered input prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-touch-filtered)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
