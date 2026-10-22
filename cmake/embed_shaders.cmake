function(ab_embed_shader INPUT_PATH SYMBOL OUTPUT_VARIABLE)
    file(READ "${INPUT_PATH}" AB_SHADER_TEXT)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${INPUT_PATH}")
    string(REPLACE "\\" "\\\\" AB_SHADER_TEXT "${AB_SHADER_TEXT}")
    string(REPLACE "\"" "\\\"" AB_SHADER_TEXT "${AB_SHADER_TEXT}")
    string(REPLACE "\n" "\\n\"\n    \"" AB_SHADER_TEXT "${AB_SHADER_TEXT}")
    set(${OUTPUT_VARIABLE}
        "static const char ${SYMBOL}[] =\n    \"${AB_SHADER_TEXT}\";\n"
        PARENT_SCOPE)
endfunction()

ab_embed_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_sprite.vert"
    ab_common_vertex_source AB_COMMON_VERTEX)
ab_embed_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_sprite.frag"
    ab_common_fragment_source AB_COMMON_FRAGMENT)
ab_embed_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_sprite.metal"
    ab_common_metal_source AB_COMMON_METAL)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/ab_common_shaders.h"
    "/* Generated from project-authored shaders. */\n"
    "${AB_COMMON_VERTEX}\n${AB_COMMON_FRAGMENT}\n${AB_COMMON_METAL}")

ab_embed_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert"
    ab_sprite_vertex_source AB_VERTEX_DECLARATION)
ab_embed_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag"
    ab_sprite_fragment_source AB_FRAGMENT_DECLARATION)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/ab_sprite_shaders.h"
    "/* Generated from project-authored shaders. */\n"
    "${AB_VERTEX_DECLARATION}\n${AB_FRAGMENT_DECLARATION}")

if(APPLE)
    ab_embed_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.metal"
        ab_sprite_metal_source AB_METAL_DECLARATION)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/ab_metal_shader.h"
        "/* Generated from the project-authored Metal shader. */\n"
        "${AB_METAL_DECLARATION}")
endif()
