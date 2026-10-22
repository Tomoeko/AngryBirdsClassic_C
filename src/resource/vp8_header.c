#include "vp8_header.h"

#include <string.h>

static uint32_t u24(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
}

static bool read_flag(AbVp8Boolean *reader, bool *output, AbError *error) {
    uint32_t value;
    if (!ab_vp8_boolean_bit(reader, 128, &value, error)) {
        return false;
    }
    *output = value != 0;
    return true;
}

static bool optional_signed(AbVp8Boolean *reader, unsigned count, int32_t *output,
                            AbError *error) {
    bool present;
    if (!read_flag(reader, &present, error)) {
        return false;
    }
    if (!present) {
        *output = 0;
        return true;
    }
    return ab_vp8_boolean_signed(reader, count, output, error);
}

static bool read_segments(AbVp8Boolean *reader, AbVp8Segments *segments,
                          AbError *error) {
    segments->absolute = true;
    memset(segments->probabilities, 255, sizeof(segments->probabilities));
    if (!read_flag(reader, &segments->enabled, error)) {
        return false;
    }
    if (!segments->enabled) {
        return true;
    }
    bool update;
    if (!read_flag(reader, &segments->update_map, error) ||
        !read_flag(reader, &update, error)) {
        return false;
    }
    if (update) {
        if (!read_flag(reader, &segments->absolute, error)) {
            return false;
        }
        for (size_t kind = 0; kind < 2; ++kind) {
            for (size_t segment = 0; segment < 4; ++segment) {
                int32_t value;
                if (!optional_signed(reader, kind ? 6 : 7, &value, error)) {
                    return false;
                }
                (kind ? segments->filters : segments->quantizers)[segment] =
                    (int8_t)value;
            }
        }
    }
    if (segments->update_map) {
        for (size_t probability = 0; probability < 3; ++probability) {
            bool present;
            uint32_t value;
            if (!read_flag(reader, &present, error) ||
                (present && !ab_vp8_boolean_value(reader, 8, &value, error))) {
                return false;
            }
            segments->probabilities[probability] = present ? (uint8_t)value : 255;
        }
    }
    return true;
}

static bool read_filter(AbVp8Boolean *reader, AbVp8Filter *filter, AbError *error) {
    uint32_t level, sharpness;
    if (!read_flag(reader, &filter->simple, error) ||
        !ab_vp8_boolean_value(reader, 6, &level, error) ||
        !ab_vp8_boolean_value(reader, 3, &sharpness, error) ||
        !read_flag(reader, &filter->use_deltas, error)) {
        return false;
    }
    filter->level = (uint8_t)level;
    filter->sharpness = (uint8_t)sharpness;
    if (!filter->use_deltas) {
        return true;
    }
    bool update;
    if (!read_flag(reader, &update, error)) {
        return false;
    }
    if (update) {
        for (size_t kind = 0; kind < 2; ++kind) {
            for (size_t index = 0; index < 4; ++index) {
                int32_t *output = kind ? filter->mode_deltas : filter->reference_deltas;
                if (!optional_signed(reader, 6, output + index, error)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static unsigned clip_quantizer(int32_t value, unsigned maximum) {
    return value < 0 ? 0 : (uint32_t)value > maximum ? maximum : (unsigned)value;
}

static bool read_quantizers(AbVp8Boolean *reader, AbVp8Header *header, AbError *error) {
    uint32_t base;
    if (!ab_vp8_boolean_value(reader, 7, &base, error)) {
        return false;
    }
    header->base_quantizer = (uint8_t)base;
    for (size_t index = 0; index < 5; ++index) {
        int32_t value;
        if (!optional_signed(reader, 4, &value, error)) {
            return false;
        }
        header->quantizer_deltas[index] = (int8_t)value;
    }
    const int8_t *delta = header->quantizer_deltas;
    for (size_t segment = 0; segment < 4; ++segment) {
        int32_t quantizer = (int32_t)base;
        if (header->segments.enabled) {
            quantizer = header->segments.quantizers[segment];
            if (!header->segments.absolute) {
                quantizer += (int32_t)base;
            }
        }
        int32_t *matrix = header->quantizers[segment];
        matrix[AB_VP8_Y1_DC] =
            ab_vp8_quantizer_dc[clip_quantizer(quantizer + delta[0], 127)];
        matrix[AB_VP8_Y1_AC] = ab_vp8_quantizer_ac[clip_quantizer(quantizer, 127)];
        matrix[AB_VP8_Y2_DC] =
            2 * ab_vp8_quantizer_dc[clip_quantizer(quantizer + delta[1], 127)];
        uint32_t y2_ac = ab_vp8_quantizer_ac[clip_quantizer(quantizer + delta[2], 127)];
        y2_ac = (y2_ac * UINT32_C(101581)) >> 16;
        matrix[AB_VP8_Y2_AC] = y2_ac < 8 ? 8 : (int32_t)y2_ac;
        matrix[AB_VP8_UV_DC] =
            ab_vp8_quantizer_dc[clip_quantizer(quantizer + delta[3], 117)];
        matrix[AB_VP8_UV_AC] =
            ab_vp8_quantizer_ac[clip_quantizer(quantizer + delta[4], 127)];
        matrix[AB_VP8_UV_QUANTIZER] = quantizer + delta[4];
    }
    return true;
}

static bool token_partitions(AbSpan frame, AbVp8Header *header, AbError *error) {
    size_t lengths = (size_t)(header->partition_count - 1) * 3;
    size_t offset = 10 + (size_t)header->first_partition_size;
    if (!ab_range(frame.size, offset, lengths)) {
        return ab_fail(error, "Truncated VP8 token partition table");
    }
    size_t cursor = offset + lengths;
    for (uint32_t index = 0; index < header->partition_count; ++index) {
        bool last = index + 1 == header->partition_count;
        size_t size = last ? frame.size - cursor : u24(frame.data + offset + index * 3);
        if (!ab_slice(frame, cursor, size, header->partitions + index) ||
            (last && !size)) {
            return ab_fail(error, "Invalid/truncated VP8 token partition extent");
        }
        cursor += size;
    }
    return true;
}

static bool read_probabilities(AbVp8Boolean *reader, AbVp8Header *header,
                               AbError *error) {
    memcpy(header->coefficient_probabilities, ab_vp8_coefficient_defaults,
           sizeof(header->coefficient_probabilities));
    for (size_t index = 0; index < AB_VP8_COEFFICIENT_PROBABILITIES; ++index) {
        uint32_t present, value;
        if (!ab_vp8_boolean_bit(reader, ab_vp8_coefficient_updates[index], &present,
                                error) ||
            (present && !ab_vp8_boolean_value(reader, 8, &value, error))) {
            return false;
        }
        if (present) {
            header->coefficient_probabilities[index] = (uint8_t)value;
        }
    }
    if (!read_flag(reader, &header->use_skip_probability, error)) {
        return false;
    }
    if (header->use_skip_probability) {
        uint32_t value;
        if (!ab_vp8_boolean_value(reader, 8, &value, error)) {
            return false;
        }
        header->skip_probability = (uint8_t)value;
    }
    return true;
}

bool ab_vp8_header_read(AbSpan frame, AbVp8Header *output, AbError *error) {
    if (!output || !frame.data || frame.size < 10 ||
        memcmp(frame.data + 3, "\x9d\x01\x2a", 3)) {
        return ab_fail(error, "Invalid VP8 key-frame header");
    }
    uint32_t tag = u24(frame.data);
    uint16_t x = ab_u16le(frame.data + 6), y = ab_u16le(frame.data + 8);
    if ((tag & 1u) || !(tag & 16u) || ((tag >> 1) & 7u) > 3) {
        return ab_fail(error, "Unsupported VP8 frame type/profile/display flag");
    }
    AbVp8Header header = {.frame = frame,
                          .width = x & 16383u,
                          .height = y & 16383u,
                          .first_partition_size = tag >> 5,
                          .profile = (uint8_t)((tag >> 1) & 7u),
                          .x_scale = (uint8_t)(x >> 14),
                          .y_scale = (uint8_t)(y >> 14)};
    header.macroblock_width = (header.width + 15) / 16;
    header.macroblock_height = (header.height + 15) / 16;
    AbSpan control;
    if (!header.width || !header.height ||
        !ab_slice(frame, 10, header.first_partition_size, &control) ||
        !ab_vp8_boolean_init(control, &header.control, error)) {
        return ab_fail(error, "Invalid VP8 dimensions/control partition");
    }
    AbVp8Boolean *reader = &header.control;
    uint32_t partition_bits;
    if (!read_flag(reader, &header.colorspace, error) ||
        !read_flag(reader, &header.clamp, error) ||
        !read_segments(reader, &header.segments, error) ||
        !read_filter(reader, &header.filter, error) ||
        !ab_vp8_boolean_value(reader, 2, &partition_bits, error)) {
        return false;
    }
    header.partition_count = 1u << partition_bits;
    if (!token_partitions(frame, &header, error) ||
        !read_quantizers(reader, &header, error) ||
        !read_flag(reader, &header.refresh_entropy, error) ||
        !read_probabilities(reader, &header, error)) {
        return false;
    }
    *output = header;
    return true;
}
