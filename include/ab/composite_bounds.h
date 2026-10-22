#ifndef AB_COMPOSITE_BOUNDS_H
#define AB_COMPOSITE_BOUNDS_H

#include "ab/physics_geometry.h"
#include "ab/sprite.h"
#include "ab/draw.h"

typedef struct {
    const AbSpriteRegion *region;
    AbVec2 offset, scale, factors;
    float rotation;
    bool enabled;
} AbCompositeBoundsPart;

typedef struct {
    int32_t width, height, pivot_x, pivot_y;
} AbCompositeBounds;

/* Native CompoSprite bounds over explicitly resolved ordinary parts. Two scale
 * vectors multiply componentwise; normalized rotation axes and sprite pivot
 * translation compose before part offset. Truncate each transformed corner to
 * int32 before collecting bounds. Empty/all-disabled input retains native
 * width/height1 and pivot INT32_MIN+1 sentinel. Disabled fields are unused.
 * Borrowed arrays, no allocation. Enabled numbers/results must be finite, angles
 * within the rotation module domain and corner coordinates in int32 range.
 * Width/pivot arithmetic preserves native modulo32 behavior. Failure preserves
 * output. Resource loading and mapping file fields to native parts are separate.
 */
bool ab_composite_bounds(const AbCompositeBoundsPart *parts, size_t count,
                         AbCompositeBounds *output, AbError *error);
/* Shared native part matrix, before integer corner truncation. */
bool ab_composite_part_transform(const AbCompositeBoundsPart *part,
                                 AbTransform2 *output);

#endif
