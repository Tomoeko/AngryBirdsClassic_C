cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_IMAGE_RESOURCES_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_IMAGE_RESOURCES_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original UI resource bytecode under Files")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare UI resource reference: ${output}${diagnostics}")
endif()
set(names set refresh size enable button angle)
set(hashes
    "a5ea0dd27818c8779a5e71ee9ed74093e6e74eaacd387db3f24174616d94c7ec"
    "c37b830a1e92ba4caffba067d22ad0b043986a26fb082f962ac94710cd4933ed"
    "662f8b35efa606931b835251f49770c72d8fd9af4408f39fcb8769d5cbf3f1b9"
    "dde0e46898e7bfb7ee1b49b381d48f6b67cb944720d23ecc5fc122947b0ff854"
    "b8e799f45e3e700c99cab23f0db574410f80e3a57a6bb373936b9817f13c3390"
    "d3f87e46083fe8efd3a59eb9e665be01d66ee44920014ac28693b5ab93ad09e4"
)
foreach(index RANGE 0 5)
    list(GET names ${index} name)
    list(GET hashes ${index} expected)
    file(SHA256 "${chunks}/image-resources-${name}-oracle.lua" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original UI resource ${name}")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${chunks}/image-resources-${name}-oracle.lua"
            "/data/local/tmp/ab520/ui/image-resources-${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage UI resource prototype: ${output}${diagnostics}")
    endif()
endforeach()
set(AB_ANDROID_ORACLE_KIND ui-image-resources)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
