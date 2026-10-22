#include <metal_stdlib>

using namespace metal;

struct SpriteParameters {
    float4 channel_multiplier;
    float2 framebuffer_size;
    float opacity;
    float coordinates_are_clip;
};

struct SpriteRaster {
    float4 position [[position]];
    float2 uv;
};

vertex SpriteRaster ab_sprite_vertex(
    uint vertex_index [[vertex_id]],
    constant float4 *vertices [[buffer(0)]],
    constant SpriteParameters &parameters [[buffer(1)]]) {
    float4 vertex_data = vertices[vertex_index];
    float2 clip_position = vertex_data.xy;
    if (parameters.coordinates_are_clip < 0.5) {
        float2 normalized_position = vertex_data.xy / parameters.framebuffer_size;
        clip_position = normalized_position * float2(2.0, -2.0);
        clip_position += float2(-1.0, 1.0);
    }
    SpriteRaster raster;
    raster.position = float4(clip_position, 0.0, 1.0);
    raster.uv = vertex_data.zw;
    return raster;
}

fragment float4 ab_sprite_fragment(
    SpriteRaster raster [[stage_in]],
    texture2d<float> sprite_texture [[texture(0)]],
    sampler sprite_sampler [[sampler(0)]],
    constant SpriteParameters &parameters [[buffer(1)]]) {
    float4 texel = sprite_texture.sample(sprite_sampler, raster.uv);
    float4 shaded = texel * parameters.channel_multiplier;
    return shaded * parameters.opacity;
}
