cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_IMAGE_STATE_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_IMAGE_STATE_ROOT}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original UI class chunks under Files")
endif()
set(names lua-base class Frame Image ImageButton)
set(paths ui/lua-base.lua animation/class.lua ui/Frame.lua ui/Image.lua ui/ImageButton.lua)
set(hashes
    "098b3e20b0d84d4f620ac738550e4989ad89180275b56eef2c82bb388b884265"
    "3c35b0b9da93fe13f308c8a3e12dc47cc5fe18a51939b1509de171ee102e4c40"
    "0ada36de30796550e7e72c961c0aa91d8e6ad374ef6efcb74e15bc3237397d9c"
    "75d24e49852395d22b7219d6de7c3ec7f9515db5435fc29b226987f1396788b6"
    "304bb5c24869a2350ab63aff993086b5ca5c5d63e1695767b52d30daccf07fda")
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare image class reference: ${output}${diagnostics}")
endif()
foreach(index RANGE 0 4)
    list(GET names ${index} name)
    list(GET paths ${index} relative)
    list(GET hashes ${index} expected)
    set(path "${chunks}/${relative}")
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original ${name} class chunk")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${path}" "/data/local/tmp/ab520/ui/ctor-${name}.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage image class chunk: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND ui-image-state)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
