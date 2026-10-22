cmake_minimum_required(VERSION 3.22)

foreach(required AB_ADB AB_ADB_SERIAL AB_REFERENCE_UI_SLIDER_DIRECTORY CHECKER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Supply ${required} explicitly")
    endif()
endforeach()
if(NOT DEFINED AB_ADB_PORT)
    set(AB_ADB_PORT 5041)
endif()
get_filename_component(private_directory "${CMAKE_CURRENT_LIST_DIR}/../Files" REALPATH)
set(adb "${AB_ADB}" -P "${AB_ADB_PORT}" -s "${AB_ADB_SERIAL}")

function(stage_slider name destination hash)
    get_filename_component(chunk
        "${AB_REFERENCE_UI_SLIDER_DIRECTORY}/runtime-hud-slider-${name}-oracle.lua" REALPATH)
    cmake_path(IS_PREFIX private_directory "${chunk}" NORMALIZE private_chunk)
    file(SHA256 "${chunk}" actual)
    if(NOT private_chunk OR NOT actual STREQUAL hash)
        message(FATAL_ERROR "Expected pinned private Slider ${name} function")
    endif()
    execute_process(COMMAND ${adb} push "${chunk}"
        "/data/local/tmp/ab520/slider-${destination}.lua"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Cannot stage Slider ${name}: ${output}${diagnostics}")
    endif()
endfunction()

stage_slider(update update
    "9ea5d0c02e809f3953c1e09841ef32fe251422480fd5ebea4a18d2392b5e8a00")
stage_slider(total total
    "ef2233315b2b2055bf5777accbdc10814bbffecce852f1af46e1abf9ad09ab89")
stage_slider(tweenLinear linear
    "569a2d69ccaeafdaecf44b237e4b2942bb79eca63640af2913305c0457c23d1d")
stage_slider(tweenEaseCubicOut out
    "e6cb261ec75c1f0de0b156b4a19b9136b10576201b12ff65b0a514cfef5f1ab5")
stage_slider(tweenEaseCubicInOut inout
    "ca9cc3844788cc79086bd9097d0d6b1be20f08b1d810594d84e3712255c8b336")
stage_slider(tweenEaseCubicIn in
    "17121f887e80306ec30a2c4f0c3ed8ebf5134043b1a69eefc7175e95dae39fd9")
set(AB_ANDROID_ORACLE_KIND ui-slider)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_ui_slider_reference.cmake")
