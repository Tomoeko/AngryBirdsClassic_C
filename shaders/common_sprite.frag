#ifdef GL_ES
precision mediump float;
#define AB_PRECISION mediump
#else
#define AB_PRECISION
#endif

varying AB_PRECISION vec2 sampled_uv;
varying AB_PRECISION vec2 sampled_mask_uv;
uniform sampler2D sprite_texture;
uniform sampler2D sprite_mask_texture;
uniform vec4 sprite_color;
uniform vec4 sprite_opacity;
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
uniform AB_CLIP_PRECISION vec4 sprite_clip;

void main() {
    if (sprite_opacity.y >= 0.5 &&
        (screen_clip_position.x < sprite_clip.x ||
         screen_clip_position.x >= sprite_clip.z ||
         screen_clip_position.y > sprite_clip.y ||
         screen_clip_position.y <= sprite_clip.w)) {
        discard;
    }
    vec4 texel = texture2D(sprite_texture, sampled_uv);
    if (sprite_opacity.z >= 0.5) {
        texel.a *= texture2D(sprite_mask_texture, sampled_mask_uv).a;
    }
    vec4 shaded = texel * sprite_color;
    gl_FragColor = shaded * sprite_opacity.x;
}
