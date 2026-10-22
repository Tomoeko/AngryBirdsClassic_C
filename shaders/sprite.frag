#version 100

#ifdef AB_HIGH_PRECISION_UV
#define AB_UV_PRECISION highp
#else
#define AB_UV_PRECISION mediump
#endif

precision mediump float;
varying AB_UV_PRECISION vec2 sampled_uv;
uniform sampler2D sprite_texture;
uniform lowp vec4 channel_multiplier;
uniform lowp float opacity;

void main() {
    lowp vec4 texel = texture2D(sprite_texture, sampled_uv);
    lowp vec4 shaded = texel * channel_multiplier;
    gl_FragColor = shaded * opacity;
}
