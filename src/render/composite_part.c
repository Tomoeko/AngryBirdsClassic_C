#include "ab/composite_part.h"

#include <math.h>
#include <string.h>

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ab_composite_part_state(const AbCompositePart *part, bool extended,
                             const AbSpriteRegion *region,
                             AbCompositeBoundsPart *output, AbError *error) {
    if (!part || !region || !output) {
        return ab_fail(error, "Missing composite part/region/state output");
    }
    AbCompositeBoundsPart state = {.region = region,
                                   .offset = {(float)part->x, (float)part->y},
                                   .scale = {1, 1},
                                   .factors = {1, 1},
                                   .enabled = true};
    if (extended) {
        state.scale = (AbVec2){number(part->scale_x_bits), number(part->scale_y_bits)};
        state.factors =
            (AbVec2){part->flip_x ? -1.0f : 1.0f, part->flip_y ? -1.0f : 1.0f};
        state.rotation = number(part->rotation_bits) * 0.0174532924f;
        if (!isfinite(state.scale.x) || !isfinite(state.scale.y) ||
            !isfinite(state.rotation)) {
            return ab_fail(error, "Nonfinite RVIO composite part transform");
        }
    }
    *output = state;
    return true;
}

bool ab_composite_part_name(const AbCompositePart *part, bool extended, char *output,
                            size_t capacity, size_t *length, AbError *error) {
    if (!part || !output || !length || !capacity ||
        (part->sprite.size && !part->sprite.data) || part->sprite.size >= capacity) {
        return ab_fail(error, "Invalid composite part name input/output bounds");
    }
    size_t size = part->sprite.size;
    bool tagged = extended && part->tag.size;
    if (tagged && (!part->tag.data || size == capacity - 1 ||
                   part->tag.size > capacity - 2 - size)) {
        return ab_fail(error, "Composite part tag exceeds name output bounds");
    }
    if (size) {
        memcpy(output, part->sprite.data, size);
    }
    if (tagged) {
        output[size++] = '#';
        memcpy(output + size, part->tag.data, part->tag.size);
        size += part->tag.size;
    }
    output[size] = 0;
    *length = size;
    return true;
}
