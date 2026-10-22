#include "ab/mp3.h"

#include <string.h>

bool ab_mp3_reservoir_push(AbMp3Reservoir *reservoir, const AbMp3FrameInfo *frame,
                           uint8_t *output, size_t capacity, AbSpan *window,
                           AbError *error) {
    if (!reservoir || !frame || !window || reservoir->size > sizeof(reservoir->bytes) ||
        (frame->main_data.size && !frame->main_data.data)) {
        return ab_fail(error, "Invalid MPEG-2 reservoir state or payload");
    }
    size_t backstep = frame->side.main_data_begin;
    if (backstep > reservoir->size || backstep > capacity ||
        frame->main_data.size > capacity - backstep ||
        ((backstep || frame->main_data.size) && !output)) {
        return ab_fail(error, "Missing MPEG-2 reservoir bytes or output capacity");
    }
    if (backstep) {
        memcpy(output, reservoir->bytes + reservoir->size - backstep, backstep);
    }
    if (frame->main_data.size) {
        memcpy(output + backstep, frame->main_data.data, frame->main_data.size);
    }
    size_t take = frame->main_data.size;
    if (take > sizeof(reservoir->bytes)) {
        take = sizeof(reservoir->bytes);
    }
    size_t keep = sizeof(reservoir->bytes) - take;
    if (keep > reservoir->size) {
        keep = reservoir->size;
    }
    if (keep) {
        memmove(reservoir->bytes, reservoir->bytes + reservoir->size - keep, keep);
    }
    if (take) {
        memcpy(reservoir->bytes + keep,
               frame->main_data.data + frame->main_data.size - take, take);
    }
    reservoir->size = keep + take;
    *window = (AbSpan){output, backstep + frame->main_data.size};
    return true;
}
