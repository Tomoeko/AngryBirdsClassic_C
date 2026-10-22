set(directory "${CMAKE_CURRENT_BINARY_DIR}/auto-key-check")
file(MAKE_DIRECTORY "${directory}")
set(output "${directory}/Level1.lua")
file(REMOVE "${output}")
execute_process(COMMAND "${TOOL}" recover-lua "${APK}"
    assets/data/levels/pack1/Level1.lua "${output}"
    RESULT_VARIABLE status OUTPUT_VARIABLE messages ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Automatic resource-key read failed: ${diagnostics}")
endif()
file(SHA256 "${output}" decoded_hash)
if(NOT decoded_hash STREQUAL
    "5f9d3c385be421a95f18ceb038d9d7d7d71eeaa982b000b83856f70d22adf8db")
    message(FATAL_ERROR "Automatic key resource recovery differs from the target")
endif()
message(STATUS "Level 1 recovered directly from APK with its recorded decoded hash")
