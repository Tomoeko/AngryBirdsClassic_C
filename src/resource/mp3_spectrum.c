#include "ab/mp3.h"

#include "mp3_huffman_data.h"

#include <string.h>

/* Cumulative spectral positions; short positions include all three windows.
 * The 16/22.05 kHz long bands coincide; their short bands remain distinct. */
static const uint16_t long_bands[2][23] = {
    {0,   6,   12,  18,  24,  30,  36,  44,  54,  66,  80, 96,
     116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576},
    {0,   6,   12,  18,  24,  30,  36,  44,  54,  66,  80, 96,
     114, 136, 162, 194, 232, 278, 332, 394, 464, 540, 576},
};

static const uint16_t short_bands[3][14] = {
    {0, 12, 24, 36, 54, 78, 108, 144, 186, 240, 312, 402, 522, 576},
    {0, 12, 24, 36, 54, 72, 96, 126, 168, 222, 300, 396, 522, 576},
    {0, 12, 24, 36, 54, 78, 108, 144, 186, 240, 312, 408, 540, 576},
};

static int sample_rate_index(uint32_t sample_rate) {
    switch (sample_rate) {
        case 16000:
            return 0;
        case 22050:
            return 1;
        case 24000:
            return 2;
        default:
            return -1;
    }
}

bool ab_mp3_spectral_limits(const AbMp3Granule *granule, uint32_t sample_rate,
                            const int16_t quantized[576], AbMp3SpectralLimits *output,
                            AbError *error) {
    int rate = sample_rate_index(sample_rate);
    if (!granule || !quantized || !output || rate < 0 || granule->block_type > 3 ||
        granule->window_switch != (granule->block_type != 0) ||
        (granule->block_type == 2 && granule->mixed_block)) {
        return ab_fail(error, "Invalid or unsupported MP3 spectral-limit layout");
    }
    for (size_t index = 0; index < 576; ++index) {
        if (quantized[index] < -8206 || quantized[index] > 8206) {
            return ab_fail(error, "MP3 spectral-limit magnitude exceeds its domain");
        }
    }
    AbMp3SpectralLimits limits = {0};
    const uint16_t *bands = granule->block_type == 2
                                ? short_bands[rate]
                                : long_bands[sample_rate == 24000 ? 1 : 0];
    size_t count = granule->block_type == 2 ? 13 : 22;
    for (size_t band = 0; band < count; ++band) {
        for (size_t index = bands[band]; index < bands[band + 1]; ++index) {
            if (!quantized[index]) {
                continue;
            }
            if (granule->block_type == 2) {
                limits.short_bands[index % 3] = (uint8_t)(band + 1);
            } else {
                limits.long_band = (uint8_t)(band + 1);
            }
        }
    }
    size_t last = limits.long_band;
    if (granule->block_type == 2) {
        for (size_t window = 0; window < 3; ++window) {
            if (limits.short_bands[window] > last) {
                last = limits.short_bands[window];
            }
        }
    }
    /* The native long bound includes an eight-line alias margin. Its short
     * table uses cumulative positions across three interleaved windows.
     * The empty spectrum still processes one subband. */
    size_t bound = granule->block_type == 2
                       ? (bands[last] ? (bands[last] - 1u) / 18u + 1u : 1u)
                       : (bands[last] + 7u) / 18u + 1u;
    limits.active_subbands = (uint8_t)(bound > 32 ? 32 : bound);
    *output = limits;
    return true;
}

bool ab_mp3_huffman_regions(const AbMp3Granule *granule, uint32_t sample_rate,
                            uint16_t pair_ends[2], AbError *error) {
    if (!granule || !pair_ends || sample_rate_index(sample_rate) < 0 ||
        granule->block_type > 3 ||
        granule->window_switch != (granule->block_type != 0) ||
        (!granule->window_switch &&
         (granule->region_count[0] > 15 || granule->region_count[1] > 7))) {
        return ab_fail(error, "Invalid MPEG-2 Huffman region metadata");
    }
    uint16_t ends[2];
    if (granule->window_switch) {
        ends[0] = granule->block_type == 2 ? 18u : 27u;
        ends[1] = 288;
    } else {
        const uint16_t *bands = long_bands[sample_rate == 24000 ? 1 : 0];
        size_t first = (size_t)granule->region_count[0] + 1;
        size_t second = first + granule->region_count[1] + 1;
        ends[0] = (uint16_t)(bands[first] / 2);
        ends[1] = second > 22 ? 288u : (uint16_t)(bands[second] / 2);
    }
    memcpy(pair_ends, ends, sizeof(ends));
    return true;
}

static void spectral_order(const AbMp3Granule *granule, int rate, uint16_t order[576]) {
    for (size_t index = 0; index < 576; ++index) {
        order[index] = (uint16_t)index;
    }
    if (granule->block_type != 2) {
        return;
    }
    size_t destination = 0;
    const uint16_t *bands = short_bands[rate];
    for (size_t band = 0; band < 13; ++band) {
        size_t width = (size_t)(bands[band + 1] - bands[band]) / 3;
        for (size_t window = 0; window < 3; ++window) {
            for (size_t index = 0; index < width; ++index) {
                order[destination++] = (uint16_t)(bands[band] + window + index * 3);
            }
        }
    }
}

bool ab_mp3_quantized_spectrum(const AbMp3Granule *granule, uint32_t sample_rate,
                               AbSpan main_data, size_t bit_offset,
                               uint16_t scalefactor_bits, int16_t output[576],
                               AbError *error) {
    uint16_t regions[2];
    if (!ab_mp3_huffman_regions(granule, sample_rate, regions, error)) {
        return false;
    }
    if (!output || main_data.size > 1024 || (main_data.size && !main_data.data) ||
        (granule->block_type == 2 && granule->mixed_block) ||
        bit_offset > main_data.size * 8 || granule->part23_length > 4095 ||
        granule->part23_length > main_data.size * 8 - bit_offset ||
        scalefactor_bits > granule->part23_length || granule->big_values > 288 ||
        granule->table_select[0] > 31 || granule->table_select[1] > 31 ||
        granule->table_select[2] > 31) {
        return ab_fail(error, "Invalid MPEG-2 quantized spectral window");
    }
    uint16_t order[576];
    spectral_order(granule, sample_rate_index(sample_rate), order);
    AbMp3HuffmanBits bits = {main_data, bit_offset + scalefactor_bits,
                             bit_offset + granule->part23_length};
    int16_t values[576] = {0};
    size_t destination = 0;
    for (size_t pair = 0; pair < granule->big_values; ++pair) {
        size_t region = pair < regions[0] ? 0u : pair < regions[1] ? 1u : 2u;
        int16_t decoded[2];
        if (!ab_mp3_huffman_pair(&bits, granule->table_select[region], decoded,
                                 error)) {
            return false;
        }
        values[order[destination++]] = decoded[0];
        values[order[destination++]] = decoded[1];
    }
    while (destination <= 572 && bits.position < bits.limit) {
        int16_t decoded[4];
        size_t count =
            ab_mp3_huffman_count1_tail(&bits, granule->count1_table, decoded);
        for (size_t index = 0; index < count; ++index) {
            values[order[destination++]] = decoded[index];
        }
        if (count < 4) {
            break;
        }
    }
    memcpy(output, values, sizeof(values));
    return true;
}

static bool scale_spectral_band(const int16_t *quantized, size_t start, size_t end,
                                size_t stride, int quarter_steps,
                                const float small_powers[16], float *output,
                                AbError *error) {
    float gain;
    if (!ab_mp3_gain_power(quarter_steps, &gain, error)) {
        return false;
    }
    for (size_t index = start; index < end; index += stride) {
        int32_t value = quantized[index];
        if (!value) {
            output[index] = 0.0f;
            continue;
        }
        uint16_t magnitude = (uint16_t)(value < 0 ? -value : value);
        float power = 0;
        if (magnitude < 16) {
            power = small_powers[magnitude];
        } else if (!ab_mp3_magnitude_power(magnitude, &power, error)) {
            return false;
        }
        float scaled = power * gain;
        output[index] = value < 0 ? -scaled : scaled;
    }
    return true;
}

bool ab_mp3_requantize_spectrum(const AbMp3Granule *granule,
                                const AbMp3Scalefactors *scalefactors,
                                uint32_t sample_rate, bool mid_side,
                                const int16_t quantized[576], float output[576],
                                AbError *error) {
    uint16_t regions[2];
    if (!ab_mp3_huffman_regions(granule, sample_rate, regions, error)) {
        return false;
    }
    bool short_block = granule->block_type == 2;
    size_t data_count = short_block ? 36u : 21u;
    size_t count = short_block ? 39u : 22u;
    if (!scalefactors || !quantized || !output ||
        (short_block && granule->mixed_block) || scalefactors->count != count ||
        scalefactors->data_count != data_count) {
        return ab_fail(error, "Invalid MPEG-2 requantization layout or storage");
    }
    for (size_t index = 0; index < count; ++index) {
        if (scalefactors->values[index] > 15 ||
            (index >= data_count && scalefactors->values[index])) {
            return ab_fail(error, "Invalid MPEG-2 requantization scalefactor");
        }
    }
    for (size_t window = 0; window < 3; ++window) {
        if (granule->subblock_gain[window] > 7) {
            return ab_fail(error, "Invalid MPEG-2 short-window gain");
        }
    }
    for (size_t index = 0; index < 576; ++index) {
        if (quantized[index] < -8206 || quantized[index] > 8206) {
            return ab_fail(error, "MPEG-2 quantized magnitude exceeds 8206");
        }
    }
    float small_powers[16], values[576];
    for (uint16_t index = 0; index < 16; ++index) {
        if (!ab_mp3_magnitude_power(index, small_powers + index, error)) {
            return false;
        }
    }
    int base = 210 - granule->global_gain + (mid_side ? 2 : 0);
    unsigned shift = granule->scalefactor_scale ? 2u : 1u;
    if (short_block) {
        const uint16_t *bands = short_bands[sample_rate_index(sample_rate)];
        for (size_t band = 0; band < 13; ++band) {
            for (size_t window = 0; window < 3; ++window) {
                int scale = scalefactors->values[band * 3 + window];
                int quarter_steps =
                    base + scale * (1 << shift) + granule->subblock_gain[window] * 8;
                if (!scale_spectral_band(quantized, bands[band] + window,
                                         bands[band + 1], 3, quarter_steps,
                                         small_powers, values, error)) {
                    return false;
                }
            }
        }
    } else {
        static const uint8_t pre_emphasis[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                               1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0};
        const uint16_t *bands = long_bands[sample_rate == 24000 ? 1 : 0];
        for (size_t band = 0; band < 22; ++band) {
            int scale = scalefactors->values[band] +
                        (scalefactors->preflag ? pre_emphasis[band] : 0);
            if (!scale_spectral_band(quantized, bands[band], bands[band + 1], 1,
                                     base + scale * (1 << shift), small_powers, values,
                                     error)) {
                return false;
            }
        }
    }
    memcpy(output, values, sizeof(values));
    return true;
}
