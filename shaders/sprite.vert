#version 100

#ifdef AB_HIGH_PRECISION_UV
#define AB_UV_PRECISION highp
#else
#define AB_UV_PRECISION mediump
#endif

attribute highp vec2 vertex_position;
attribute AB_UV_PRECISION vec2 vertex_uv;
uniform highp vec2 framebuffer_size;
uniform lowp float coordinates_are_clip;
varying AB_UV_PRECISION vec2 sampled_uv;

void main() {
    highp vec2 clip_position = vertex_position;
    if (coordinates_are_clip < 0.5) {
        highp vec2 normalized_position = vertex_position / framebuffer_size;
        clip_position = normalized_position * vec2(2.0, -2.0);
        clip_position += vec2(-1.0, 1.0);
    }
    gl_Position = vec4(clip_position, 0.0, 1.0);
    sampled_uv = vertex_uv;
}
