#include "ab/composite_bounds.h"
#include "ab/draw.h"
#include "ab/rotation.h"

#include <math.h>
#include <string.h>

static AbVec2 normalize(AbVec2 vector) {
    float squared = vector.y * vector.y;
    squared += vector.x * vector.x;
    float reciprocal = 1 / sqrtf(squared);
    return (AbVec2){reciprocal * vector.x, reciprocal * vector.y};
}

bool ab_composite_part_transform(const AbCompositeBoundsPart *part,
                                 AbTransform2 *output) {
    AbRotation rotation;
    if (!part || !output || !part->region || !isfinite(part->offset.x) ||
        !isfinite(part->offset.y) || !isfinite(part->scale.x) ||
        !isfinite(part->scale.y) || !isfinite(part->factors.x) ||
        !isfinite(part->factors.y) ||
        !ab_rotation_from_angle(part->rotation, &rotation)) {
        return false;
    }
    AbVec2 horizontal = normalize((AbVec2){rotation.cosine, rotation.sine});
    AbVec2 vertical = normalize((AbVec2){-rotation.sine, rotation.cosine});
    float sx = part->scale.x * part->factors.x;
    float sy = part->scale.y * part->factors.y;
    float xx = sx * horizontal.x, yx = sx * horizontal.y;
    float xy = sy * vertical.x, yy = sy * vertical.y;
    int32_t pivot_x = (int32_t)((float)part->region->pivot_x + 0.5f);
    int32_t pivot_y = (int32_t)((float)part->region->pivot_y + 0.5f);
    float tx = xy * (float)-pivot_y;
    tx += xx * (float)-pivot_x;
    tx += 0;
    float ty = yy * (float)-pivot_y;
    ty += yx * (float)-pivot_x;
    ty += 0;
    AbTransform2 transform;
    transform.xx = xy * 0;
    transform.xx += xx;
    transform.xy = xy;
    transform.xy += xx * 0;
    transform.yx = yy * 0;
    transform.yx += yx;
    transform.yy = yy;
    transform.yy += yx * 0;
    transform.tx = tx + part->offset.x;
    transform.ty = ty + part->offset.y;
    if (!isfinite(transform.xx) || !isfinite(transform.xy) || !isfinite(transform.yx) ||
        !isfinite(transform.yy) || !isfinite(transform.tx) || !isfinite(transform.ty)) {
        return false;
    }
    *output = transform;
    return true;
}

static bool integer_corner(AbVec2 corner, AbTransform2 transform, int32_t output[2]) {
    float x = corner.y * transform.xy;
    x += corner.x * transform.xx;
    x += transform.tx;
    float y = corner.y * transform.yy;
    y += corner.x * transform.yx;
    y += transform.ty;
    if (!isfinite(x) || !isfinite(y) || x < -0x1p31f || x >= 0x1p31f || y < -0x1p31f ||
        y >= 0x1p31f) {
        return false;
    }
    output[0] = (int32_t)x;
    output[1] = (int32_t)y;
    return true;
}

static int32_t signed_word(uint32_t word) {
    int32_t value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

bool ab_composite_bounds(const AbCompositeBoundsPart *parts, size_t count,
                         AbCompositeBounds *output, AbError *error) {
    if (!output || (count && !parts)) {
        return ab_fail(error, "Invalid composite bounds input/output");
    }
    int32_t minimum[2] = {INT32_MAX, INT32_MAX};
    int32_t maximum[2] = {INT32_MIN, INT32_MIN};
    for (size_t index = 0; index < count; ++index) {
        const AbCompositeBoundsPart *part = parts + index;
        if (!part->enabled) {
            continue;
        }
        AbTransform2 transform;
        if (!ab_composite_part_transform(part, &transform)) {
            return ab_fail(error, "Unsupported composite bounds transform");
        }
        const AbVec2 corners[] = {{0, 0},
                                  {part->region->width, 0},
                                  {0, part->region->height},
                                  {part->region->width, part->region->height}};
        for (size_t corner = 0; corner < 4; ++corner) {
            int32_t point[2];
            if (!integer_corner(corners[corner], transform, point)) {
                return ab_fail(error, "Composite corner exceeds int32 range");
            }
            for (size_t axis = 0; axis < 2; ++axis) {
                if (point[axis] < minimum[axis]) {
                    minimum[axis] = point[axis];
                }
                if (point[axis] > maximum[axis]) {
                    maximum[axis] = point[axis];
                }
            }
        }
    }
    *output = (AbCompositeBounds){
        signed_word((uint32_t)maximum[0] - (uint32_t)minimum[0]),
        signed_word((uint32_t)maximum[1] - (uint32_t)minimum[1]),
        signed_word(0u - (uint32_t)minimum[0]), signed_word(0u - (uint32_t)minimum[1])};
    return true;
}
