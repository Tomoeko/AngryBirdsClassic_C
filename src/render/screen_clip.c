#include "ab/screen_clip.h"

#include <math.h>

static bool edge(float value, int32_t *output) {
    float rounded = roundf(value);
    if (!isfinite(rounded) || rounded < -0x1p31f || rounded >= 0x1p31f)
        return false;
    *output = (int32_t)rounded;
    return true;
}

bool ab_screen_clip(AbScreenClipRect request, const AbVec2 *offset, AbDrawClip *output,
                    AbError *error) {
    AbVec2 origin = offset ? *offset : (AbVec2){0};
    if (!output || !isfinite(origin.x) || !isfinite(origin.y) || !isfinite(request.x) ||
        !isfinite(request.y) || !isfinite(request.width) || !isfinite(request.height))
        return ab_fail(error, "Invalid screen clip request/offset");
    float left = request.x + origin.x;
    float top = request.y + origin.y;
    float right = left + request.width;
    float bottom = top + request.height;
    AbDrawClip clip = {.enabled = true};
    if (!edge(left, &clip.left) || !edge(right, &clip.right) || !edge(top, &clip.top) ||
        !edge(bottom, &clip.bottom) || clip.right < clip.left || clip.bottom < clip.top)
        return ab_fail(error, "Unsupported screen clip integer bounds");
    *output = clip;
    return true;
}

static uint32_t bounded(int32_t value, uint32_t extent) {
    return value <= 0 ? 0 : (uint32_t)value < extent ? (uint32_t)value : extent;
}

bool ab_draw_clip_scissor(AbDrawClip clip, uint32_t width, uint32_t height,
                          AbDrawScissor *output, AbError *error) {
    if (!output || !width || !height || width > INT32_MAX || height > INT32_MAX ||
        (clip.enabled && (clip.right < clip.left || clip.bottom < clip.top)))
        return ab_fail(error, "Invalid draw clip/framebuffer bounds");
    AbDrawScissor result = {0, 0, width, height};
    if (clip.enabled) {
        result.x = bounded(clip.left, width);
        result.y = bounded(clip.top, height);
        result.width = bounded(clip.right, width) - result.x;
        result.height = bounded(clip.bottom, height) - result.y;
    }
    *output = result;
    return true;
}
