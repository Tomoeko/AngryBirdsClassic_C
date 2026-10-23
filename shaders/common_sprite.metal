#include <metal_stdlib>

using namespace metal;

struct SpriteInput {
    float4 xyuv [[attribute(0)]];
    float4 mask_uv [[attribute(1)]];
};

struct SpriteOutput {
    float4 position [[position]];
    float2 uv;
    float2 mask_uv;
    float2 screen_clip_position;
};

vertex SpriteOutput ab_common_vertex(SpriteInput input [[stage_in]],
                                     constant float4 &viewport [[buffer(1)]]) {
    float2 clip_position = input.xyuv.xy;
    if (viewport.z < 0.5) {
        float2 normalized_position = input.xyuv.xy / viewport.xy;
        clip_position = normalized_position * float2(2.0, -2.0);
        clip_position += float2(-1.0, 1.0);
    }
    SpriteOutput output;
    output.position = float4(clip_position, 0.0, 1.0);
    output.uv = input.xyuv.zw;
    output.mask_uv = input.mask_uv.xy;
    output.screen_clip_position = clip_position;
    return output;
}

fragment float4 ab_common_fragment(SpriteOutput input [[stage_in]],
                                   constant float4 *parameters [[buffer(2)]],
                                   texture2d<float> image [[texture(0)]],
                                   sampler image_sampler [[sampler(0)]],
                                   texture2d<float> mask [[texture(1)]],
                                   sampler mask_sampler [[sampler(1)]]) {
    float4 clip = parameters[2];
    if (parameters[1].y >= 0.5 &&
        (input.screen_clip_position.x < clip.x ||
         input.screen_clip_position.x >= clip.z ||
         input.screen_clip_position.y > clip.y ||
         input.screen_clip_position.y <= clip.w)) {
        discard_fragment();
    }
    float4 texel = image.sample(image_sampler, input.uv);
    if (parameters[1].z >= 0.5) {
        texel.a *= mask.sample(mask_sampler, input.mask_uv).a;
    }
    float4 shaded = texel * parameters[0];
    return shaded * parameters[1].x;
}
