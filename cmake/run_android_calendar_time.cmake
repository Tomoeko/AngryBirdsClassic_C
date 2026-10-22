cmake_minimum_required(VERSION 3.22)

set(AB_ANDROID_ORACLE_KIND calendar-time)
include("${CMAKE_CURRENT_LIST_DIR}/run_android_oracle.cmake")
set(REFERENCE "${AB_ANDROID_TRACE}")
include("${CMAKE_CURRENT_LIST_DIR}/check_calendar_time_reference.cmake")
