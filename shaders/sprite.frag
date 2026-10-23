#version 100

#ifdef AB_HIGH_PRECISION_UV
#define AB_UV_PRECISION highp
#else
#define AB_UV_PRECISION mediump
#endif

precision mediump float;
varying AB_UV_PRECISION vec2 sampled_uv;
varying AB_UV_PRECISION vec2 sampled_mask_uv;
uniform sampler2D sprite_texture;
uniform sampler2D mask_texture;
uniform lowp float mask_enabled;
uniform lowp vec4 channel_multiplier;
uniform lowp float opacity;

void main() {
    lowp vec4 texel = texture2D(sprite_texture, sampled_uv);
    if (mask_enabled >= 0.5) {
        texel.a *= texture2D(mask_texture, sampled_mask_uv).a;
    }
    lowp vec4 shaded = texel * channel_multiplier;
    gl_FragColor = shaded * opacity;
}
