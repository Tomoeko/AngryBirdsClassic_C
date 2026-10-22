#include "ab/rubberband.h"

#include <math.h>
#include <string.h>

bool ab_rubberband_project(const AbRubberband *world, AbVec2 camera_top_left,
                           float zoom, AbRubberband *screen, AbError *error) {
    if (!world || !screen || !isfinite(world->start.x) || !isfinite(world->start.y) ||
        !isfinite(world->end.x) || !isfinite(world->end.y) ||
        !isfinite(camera_top_left.x) || !isfinite(camera_top_left.y) ||
        !isfinite(zoom)) {
        return ab_fail(error, "Invalid world rubberband projection");
    }
    AbRubberband result = *world;
    result.start.x = (world->start.x - camera_top_left.x) * zoom;
    result.start.y = (world->start.y - camera_top_left.y) * zoom;
    result.end.x = (world->end.x - camera_top_left.x) * zoom;
    result.end.y = (world->end.y - camera_top_left.y) * zoom;
    result.width = world->width * zoom;
    if (!isfinite(result.start.x) || !isfinite(result.start.y) ||
        !isfinite(result.end.x) || !isfinite(result.end.y)) {
        return ab_fail(error, "Rubberband projection exceeds finite range");
    }
    *screen = result;
    return true;
}

bool ab_rubberband_world_vertices(const AbRubberband *world, AbVec2 camera_top_left,
                                  float zoom, AbVec2 vertices[4], bool *drawn,
                                  AbError *error) {
    AbRubberband screen = {0};
    return ab_rubberband_project(world, camera_top_left, zoom, &screen, error) &&
           ab_rubberband_vertices(&screen, vertices, drawn, error);
}

bool ab_rubberband_vertices(const AbRubberband *band, AbVec2 vertices[4], bool *drawn,
                            AbError *error) {
    if (!band || !vertices || !drawn || !isfinite(band->start.x) ||
        !isfinite(band->start.y) || !isfinite(band->end.x) || !isfinite(band->end.y)) {
        return ab_fail(error, "Invalid rubberband endpoints or outputs");
    }
    float dy = band->end.y - band->start.y;
    float dx = band->end.x - band->start.x;
    float squared = dy * dy;
    squared += dx * dx;
    if (squared == 0) {
        *drawn = false;
        return true;
    }
    if (!isfinite(dx) || !isfinite(dy) || !isfinite(band->width)) {
        return ab_fail(error, "Unsupported nonfinite rubberband geometry");
    }
    float factor = (band->width * 0.5f) / sqrtf(squared);
    float offset_x = factor * dy;
    float offset_y = factor * dx;
    AbVec2 points[4] = {{band->start.x + offset_x, band->start.y - offset_y},
                        {band->end.x + offset_x, band->end.y - offset_y},
                        {band->start.x - offset_x, band->start.y + offset_y},
                        {band->end.x - offset_x, band->end.y + offset_y}};
    for (size_t index = 0; index < 4; ++index) {
        /* Sprite.draw evaluates this identity route, including zero products.
         * Its final additions normalize signed zero before the image boundary. */
        float x = points[index].y * 0.0f;
        x += points[index].x * 1.0f;
        x += 0.0f;
        float y = points[index].y * 1.0f;
        y += points[index].x * 0.0f;
        y += 0.0f;
        if (!isfinite(x) || !isfinite(y)) {
            return ab_fail(error, "Rubberband image vertices exceed finite range");
        }
        points[index] = (AbVec2){x, y};
    }
    memcpy(vertices, points, sizeof(points));
    *drawn = true;
    return true;
}
