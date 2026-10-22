#ifndef AB_ORACLE_MP3_HUFFMAN_H
#define AB_ORACLE_MP3_HUFFMAN_H

static int (*mp3_native_dequant)(void *, float *, int32_t *, uint32_t *, int, int);
#if !defined(AB_ORACLE_MP3_REQUANTIZE)
static float mp3_huffman_powers[8207];
#endif
#if defined(AB_ORACLE_MP3_SPECTRUM) || defined(AB_ORACLE_MP3_REQUANTIZE)
enum { MP3_SPECTRAL_VALUES = 576, MP3_SPECTRAL_WORDS = 1155 };
#else
enum { MP3_SPECTRAL_VALUES = 4, MP3_SPECTRAL_WORDS = 11 };
#endif
static uint32_t mp3_huffman_observation[MP3_SPECTRAL_WORDS];
#if defined(AB_ORACLE_MP3_REQUANTIZE)
static uint32_t mp3_requantize_control[41];
#endif

static void initialize_mp3_huffman(void) {
    uintptr_t bias = (uintptr_t)find_function("INT123_do_layer3") - 0x8fc60c;
    mp3_native_dequant =
        (int (*)(void *, float *, int32_t *, uint32_t *, int, int))(bias + 0x8f92b8);
#if !defined(AB_ORACLE_MP3_REQUANTIZE)
    double (*power)(double, double) = (double (*)(double, double))find_function("pow");
    /* The original initialization converts pow(q, 4/3) to float. This private
     * dictionary identifies exact native output values; it is not runtime data. */
    for (size_t index = 0; index < 8207; ++index) {
        mp3_huffman_powers[index] = (float)power((double)index, 4.0 / 3.0);
    }
#endif
}

#if !defined(AB_ORACLE_MP3_REQUANTIZE)
static uint32_t identify_mp3_huffman(float value, float gain) {
    union {
        float number;
        uint32_t bits;
    } observed = {.number = value};
    uint32_t negative = observed.bits >> 31;
    observed.bits &= UINT32_C(0x7fffffff);
    size_t first = 0, end = 8207;
    while (first < end) {
        size_t middle = first + (end - first) / 2;
        float candidate = mp3_huffman_powers[middle] * gain;
        if (candidate < observed.number) {
            first = middle + 1;
        } else {
            end = middle;
        }
    }
    union {
        float number;
        uint32_t bits;
    } expected = {.number = first < 8207 ? mp3_huffman_powers[first] * gain : -1.0f};
    if (first == 8207 || expected.bits != observed.bits || (!first && negative)) {
        fail("Native Huffman coefficient has no exact magnitude dictionary match\n");
    }
    return negative ? 0u - (uint32_t)first : (uint32_t)first;
}
#endif

static void capture_mp3_huffman(uint32_t *decoder, const uint32_t *side) {
    if (decoder[0x12f0] != 1 || side[5] || side[18] || side[4] > 288) {
        fail("Native Huffman probe requires one unscaled mono granule\n");
    }
#if !defined(AB_ORACLE_MP3_SPECTRUM) && !defined(AB_ORACLE_MP3_REQUANTIZE)
    if (side[6] || side[4] > 1) {
        fail("Native pair/quad probe requires one long-block pair or quad\n");
    }
#endif
    uint32_t granule[23];
    for (size_t index = 0; index < 23; ++index) {
        granule[index] = side[index + 2];
    }
    int32_t scalefactors[39] = {0};
#if defined(AB_ORACLE_MP3_REQUANTIZE)
    /* Explicit unit inputs to the private core. The packet still carries zero
     * scalefactor bits; these printed argument values exercise band scaling
     * separately from the previously compared LSF partition reader. */
    uint32_t frame = decoder[0x4c24 / 4];
    size_t data_count = granule[4] == 2 ? 36u : 21u;
    mp3_requantize_control[0] = frame & 1u;
    mp3_requantize_control[1] = frame >> 1 & 1u;
    granule[16] = mp3_requantize_control[0];
    for (size_t index = 0; index < 39; ++index) {
        uint32_t value =
            index < data_count ? (frame + (uint32_t)index * 7 + (frame >> 4)) & 15u : 0;
        scalefactors[index] = (int32_t)value;
        mp3_requantize_control[2 + index] = value;
    }
    if (mp3_requantize_control[1]) {
        granule[22] += 8;
        if (granule[4] == 2) {
            for (size_t window = 0; window < 3; ++window) {
                granule[19 + window] += 8;
            }
        }
    }
#endif
    _Alignas(16) float coefficients[640] = {0};
    for (size_t index = 576; index < 640; ++index) {
        coefficients[index] = 1234.25f;
    }
    uint32_t saved[4];
    for (size_t index = 0; index < 4; ++index) {
        saved[index] = decoder[0x4c48 / 4 + index];
    }
    uint8_t *window =
        (uint8_t *)(uintptr_t)decoder[0x6ba0 / 4] + decoder[0x4c98 / 4] - side[0];
    decoder[0x4c48 / 4] = 0;
    decoder[0x4c4c / 4] = (uint32_t)(uintptr_t)window;
    int status = mp3_native_dequant(decoder, coefficients, scalefactors, granule,
                                    (int)decoder[0x12fe], 0);
    uint32_t consumed =
        (decoder[0x4c4c / 4] - (uint32_t)(uintptr_t)window) * 8 + decoder[0x4c48 / 4];
    for (size_t index = 0; index < 4; ++index) {
        decoder[0x4c48 / 4 + index] = saved[index];
    }
    union {
        float number;
        uint32_t bits;
    } gain = {.number = *(const float *)(uintptr_t)granule[22]};
    mp3_huffman_observation[0] = (uint32_t)status;
    mp3_huffman_observation[1] = consumed;
    mp3_huffman_observation[2] = gain.bits;
    for (size_t index = 0; index < MP3_SPECTRAL_VALUES; ++index) {
        union {
            float number;
            uint32_t bits;
        } value = {.number = coefficients[index]};
        mp3_huffman_observation[3 + index] = value.bits;
#if !defined(AB_ORACLE_MP3_REQUANTIZE)
        mp3_huffman_observation[3 + MP3_SPECTRAL_VALUES + index] =
            identify_mp3_huffman(value.number, gain.number);
#endif
    }
    for (size_t index = MP3_SPECTRAL_VALUES; index < 576; ++index) {
        if (coefficients[index] != 0.0f) {
            fail("Native Huffman probe produced an unexpected spectral tail\n");
        }
    }
    for (size_t index = 576; index < 640; ++index) {
        if (coefficients[index] != 1234.25f) {
            fail("Native spectral core changed the private output guard\n");
        }
    }
}

#endif
