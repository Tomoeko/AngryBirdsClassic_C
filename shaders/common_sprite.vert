#ifdef GL_ES
#define AB_PRECISION highp
#define AB_UV_PRECISION mediump
#else
#define AB_PRECISION
#define AB_UV_PRECISION
#endif

attribute AB_PRECISION vec4 sprite_vertex;
attribute AB_UV_PRECISION vec4 sprite_mask_uv;
uniform AB_PRECISION vec4 sprite_viewport;
varying AB_UV_PRECISION vec2 sampled_uv;
varying AB_UV_PRECISION vec2 sampled_mask_uv;
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
#define AB_CLIP_PRECISION highp
#else
#define AB_CLIP_PRECISION mediump
#endif
#else
#define AB_CLIP_PRECISION
#endif
varying AB_CLIP_PRECISION vec2 screen_clip_position;

void main() {
    AB_PRECISION vec2 clip_position = sprite_vertex.xy;
    if (sprite_viewport.z < 0.5) {
        AB_PRECISION vec2 normalized_position = sprite_vertex.xy / sprite_viewport.xy;
        clip_position = normalized_position * vec2(2.0, -2.0);
        clip_position += vec2(-1.0, 1.0);
    }
    gl_Position = vec4(clip_position, 0.0, 1.0);
    sampled_uv = sprite_vertex.zw;
    sampled_mask_uv = sprite_mask_uv.xy;
    screen_clip_position = clip_position;
}
