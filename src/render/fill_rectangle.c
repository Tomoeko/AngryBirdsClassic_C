#include "ab/fill_rectangle.h"
#include "ab/image_points.h"

#include <string.h>

bool ab_fill_rectangle_reset_command(AbFillRectangle rectangle, uint32_t viewport_width,
                                     uint32_t viewport_height, uint32_t white_texture,
                                     AbDrawCommand *output, bool *visible,
                                     AbError *error) {
    if (!output || !visible || (unsigned)rectangle.blend > AB_FILL_BLEND_OPAQUE) {
        return ab_fail(error, "Invalid reset rectangle output/blend");
    }
    float left = (float)rectangle.left;
    float top = (float)rectangle.top;
    float right = left + (float)rectangle.width;
    float bottom = top + (float)rectangle.height;
    const AbVec2 points[] = {
        {left, top}, {right, top}, {left, bottom}, {right, bottom}};
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    AbDrawQuad quad = {0};
    bool drawn = false;
    if (!ab_image_points_quad(points, viewport_width, viewport_height, uv, &quad,
                              &drawn, error)) {
        return false;
    }
    if (!drawn) {
        *visible = false;
        return true;
    }
    if (!white_texture) {
        return ab_fail(error, "Missing solid-white rectangle texture");
    }
    const uint32_t reciprocal_word = UINT32_C(0x3b808081);
    float reciprocal;
    memcpy(&reciprocal, &reciprocal_word, sizeof(reciprocal));
    const unsigned shifts[] = {16, 8, 0, 24};
    AbDrawCommand command = {.texture = white_texture,
                             .quad = quad,
                             .opacity = 1,
                             .coordinates = AB_DRAW_CLIP};
    for (size_t channel = 0; channel < 4; ++channel) {
        command.color[channel] = (float)((rectangle.argb >> shifts[channel]) & 255);
        command.color[channel] *= reciprocal;
    }
    bool alpha = rectangle.blend == AB_FILL_BLEND_ALPHA ||
                 (rectangle.blend == AB_FILL_BLEND_AUTO && rectangle.argb >> 24 != 255);
    command.blend = alpha ? AB_BLEND_STRAIGHT : AB_BLEND_OPAQUE;
    *output = command;
    *visible = true;
    return true;
}
