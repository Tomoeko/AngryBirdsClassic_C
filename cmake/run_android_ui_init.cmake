cmake_minimum_required(VERSION 3.22)
foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_INIT_CHUNKS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
get_filename_component(chunks "${AB_REFERENCE_UI_INIT_CHUNKS}" REALPATH)
cmake_path(IS_PREFIX private_directory "${chunks}" NORMALIZE private_chunks)
if(NOT private_chunks)
    message(FATAL_ERROR "Keep original UI constructor bytecode under Files")
endif()
set(functions class-new frame-init text-init)
set(hashes
    "0f42926a3cae43a1e42ae90b56d135c0e73ba931b53e2c7ac991fbfc302a0223"
    "93a25a98711bfa48cbe663fe9f1cf98df6287b2b5fd223d6695346241f6ace45"
    "050df6959cf999ef88d6d948a54c52442536c8d0acee01bfc6ab052b89783137")
if(AB_UI_INIT_INCLUDE_TEXT_LINES OR AB_UI_INIT_INCLUDE_TEXT_METRICS OR AB_UI_INIT_INCLUDE_TEXT_BOUNDS)
    list(APPEND functions frame-scale text-clip)
    list(APPEND hashes
        "95cd7997ff17644e54737568ff271fc4a140b4fc8d39803d435d6288f5085fe4"
        "b1b65f084e317a8228152b0c28cf55b3907ecfc11a140c13b2afc4f732673c6d")
elseif(AB_UI_INIT_INCLUDE_FONT)
    list(APPEND functions text-font text-layout)
    list(APPEND hashes
        "26205ed3f9cce8c312dd1e03329be6022534aabe26cd45d84a5d553b8d77e13f"
        "f64fdf3910139ea1374f38c5cc5d10ebbad211cfea8b7a70fb99a7c5dfbedcfc")
endif()
if(AB_UI_INIT_INCLUDE_TEXT_METRICS)
    list(APPEND functions text-width text-height text-leading)
    list(APPEND hashes
        "1d62c4af54cd817ca2c4f2e08f08163845e53bfc3c5578275e46de33967a92ca"
        "48af1025830be2d8d3875e05edf6f16bb85f0619ede09f0bcfe221a7fa9e1da7"
        "aa5340ef22f67c3f25c0003474c52bbe7fcc5fd88b10b39917fb5513cb1baa91")
endif()
if(AB_UI_INIT_INCLUDE_TEXT_BOUNDS)
    list(APPEND functions text-bounds text-correction)
    list(APPEND hashes
        "eb70508b4c4b51a0ec37b8f9e0158d8474ada8915f679a9d921055a0aecc3d68"
        "8df5a05df26a21cd40ac496ca44db9c4c0d7b9622b1981393f13b38095063284")
endif()
execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
    shell mkdir -p /data/local/tmp/ab520/ui
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Cannot prepare UI initialization reference: ${output}${diagnostics}")
endif()
list(LENGTH functions function_count)
math(EXPR last_function "${function_count} - 1")
foreach(index RANGE 0 ${last_function})
    list(GET functions ${index} name)
    list(GET hashes ${index} expected)
    set(path "${chunks}/${name}-oracle.lua")
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected pinned original ${name} prototype")
    endif()
    execute_process(COMMAND "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}"
        push "${path}" "/data/local/tmp/ab520/ui/${name}-oracle.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage UI constructor: ${output}${diagnostics}")
    endif()
endforeach()
if(AB_UI_INIT_INCLUDE_TEXT_BOUNDS)
    set(AB_ANDROID_ORACLE_KIND ui-text-bounds)
elseif(AB_UI_INIT_INCLUDE_TEXT_METRICS)
    set(AB_ANDROID_ORACLE_KIND ui-text-metrics)
elseif(AB_UI_INIT_INCLUDE_TEXT_LINES)
    set(AB_ANDROID_ORACLE_KIND ui-text-lines)
elseif(AB_UI_INIT_INCLUDE_FONT)
    set(AB_ANDROID_ORACLE_KIND ui-font)
else()
    set(AB_ANDROID_ORACLE_KIND ui-init)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
