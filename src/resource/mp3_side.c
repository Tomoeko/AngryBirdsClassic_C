#include "ab/mp3.h"

typedef struct {
    AbSpan input;
    size_t bit;
    bool valid;
} SideReader;

static uint32_t read_bits(SideReader *reader, unsigned count) {
    if (count > reader->input.size * 8 - reader->bit) {
        reader->valid = false;
        return 0;
    }
    uint32_t value = 0;
    for (unsigned index = 0; index < count; ++index) {
        size_t bit = reader->bit++;
        unsigned shift = 7u - (unsigned)(bit % 8);
        value = value * 2 + ((uint32_t)reader->input.data[bit / 8] >> shift & 1u);
    }
    return value;
}

static bool read_granule(SideReader *reader, AbMp3Granule *granule, AbError *error) {
    granule->part23_length = (uint16_t)read_bits(reader, 12);
    granule->big_values = (uint16_t)read_bits(reader, 9);
    granule->global_gain = (uint8_t)read_bits(reader, 8);
    granule->scalefactor_compress = (uint16_t)read_bits(reader, 9);
    granule->window_switch = read_bits(reader, 1) != 0;
    if (granule->big_values > 288) {
        return ab_fail(error, "MP3 side-information big-values count exceeds 288");
    }
    if (granule->window_switch) {
        granule->block_type = (uint8_t)read_bits(reader, 2);
        granule->mixed_block = read_bits(reader, 1) != 0;
        if (!granule->block_type) {
            return ab_fail(error, "MP3 switched window has block type zero");
        }
        for (size_t index = 0; index < 2; ++index) {
            granule->table_select[index] = (uint8_t)read_bits(reader, 5);
        }
        for (size_t index = 0; index < 3; ++index) {
            granule->subblock_gain[index] = (uint8_t)read_bits(reader, 3);
        }
    } else {
        for (size_t index = 0; index < 3; ++index) {
            granule->table_select[index] = (uint8_t)read_bits(reader, 5);
        }
        granule->region_count[0] = (uint8_t)read_bits(reader, 4);
        granule->region_count[1] = (uint8_t)read_bits(reader, 3);
    }
    granule->scalefactor_scale = read_bits(reader, 1) != 0;
    granule->count1_table = read_bits(reader, 1) != 0;
    return true;
}

bool ab_mp3_side_info(AbSpan input, uint16_t channels, AbMp3SideInfo *output,
                      AbError *error) {
    if (!output || !input.data || (channels != 1 && channels != 2) ||
        input.size != (channels == 1 ? 9u : 17u)) {
        return ab_fail(error, "Invalid MPEG-2 side-information storage or channels");
    }
    SideReader reader = {input, 0, true};
    AbMp3SideInfo side = {.channels = channels};
    side.main_data_begin = (uint16_t)read_bits(&reader, 8);
    side.private_bits = (uint8_t)read_bits(&reader, channels);
    for (size_t index = 0; index < channels; ++index) {
        if (!read_granule(&reader, side.channel + index, error)) {
            return false;
        }
    }
    if (!reader.valid || reader.bit != input.size * 8) {
        return ab_fail(error, "Invalid MPEG-2 side-information bit count");
    }
    *output = side;
    return true;
}
