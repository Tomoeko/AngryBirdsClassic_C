cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED CHECKER OR "${CHECKER}" STREQUAL "")
    message(FATAL_ERROR "Supply CHECKER explicitly")
endif()
set(AB_ANDROID_ORACLE_KIND ui-native-value-text)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_ui_native_value_text_reference.cmake")
