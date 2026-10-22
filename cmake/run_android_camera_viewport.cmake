cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_CAMERA_VIEWPORT_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_CAMERA_VIEWPORT_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original camera-viewport-reference chunks under Files")
endif()
set(chunk_names
    viewport-left viewport-top viewport-right viewport-bottom
    viewport-width viewport-height viewport-render-left viewport-render-top)
set(chunk_hashes
    "18c7dab6efa5cc9d57094110503db1ac41ebdedb75350eb90a0de00027a50f56"
    "c7bf8d1443ba2c748486e1880ddc1187c317730f9257f838e1eaf0414aa36964"
    "ea711833929666ba43b26f836210b856fd1e1870445333c1a71793309c76f356"
    "0c1672cffec2ad6d6758d5281982c8d4dd4352f320a04bd03148f252d1860988"
    "2788e8eb731b1fa6e43ec547f12a4085884d9b73ce776c120e23fcce4b55f54f"
    "01673e751e5f3efd8a8fe4aaee74bcc16d012caaa96bacf30b2fe4f117dd4ee7"
    "fd3e9902b4f6d03393420c25a8759770f97f0d06bc5dc479109d6f404f61ed98"
    "ab1cc87bd454157d186512eac7cab47bca8d4adea1f3fd13a6bdf9dde9ae93fb"
)
foreach(index RANGE 0 7)
    list(GET chunk_names ${index} name)
    list(GET chunk_hashes ${index} expected_hash)
    file(SHA256 "${chunks}/${name}-oracle.lua" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Expected the recovered target ${name} prototype")
    endif()
endforeach()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/viewport RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare camera-viewport oracle: ${output}${diagnostics}")
endif()
foreach(name IN LISTS chunk_names)
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/${name}-oracle.lua" "/data/local/tmp/ab520/viewport/${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage camera-viewport prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND camera-viewport)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
