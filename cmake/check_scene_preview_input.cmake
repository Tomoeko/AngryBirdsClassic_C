set(directory "${CMAKE_CURRENT_BINARY_DIR}/scene-input-check")
file(MAKE_DIRECTORY "${directory}")

function(reject_scene name contents expected)
    set(path "${directory}/${name}.txt")
    file(WRITE "${path}" "${contents}")
    execute_process(COMMAND "${PREVIEW}" --scene "${path}" "${APK}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics
        TIMEOUT 15)
    if(NOT status STREQUAL "1" OR NOT output STREQUAL "")
        message(FATAL_ERROR "Invalid scene ${name} did not reject before presentation")
    endif()
    string(FIND "${diagnostics}" "${expected}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Invalid scene ${name} returned an unexpected diagnostic")
    endif()
endfunction()

reject_scene(header "AB_SPRITE_SCENE_V0 1\n" "Invalid scene preview header")
reject_scene(overflow "AB_SPRITE_SCENE_V1 184467440737095516160\n"
    "Invalid scene preview header")
set(prefix "AB_SPRITE_SCENE_V1 1\n00000000 00000000 3f800000 3f800000\n")
reject_scene(layer "${prefix}sheet sprite 9223372036854775808\n"
    "Invalid signed scene layer")
reject_scene(flags "${prefix}sheet sprite 0 0 2\n" "Invalid unsigned scene field")
string(REPEAT "x" 1024 long_token)
reject_scene(token "${prefix}${long_token}\n" "Scene token exceeds")
message(STATUS "Scene preview rejects invalid headers, integer overflow, flags and long tokens")
