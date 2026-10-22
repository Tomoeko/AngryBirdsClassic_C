#ifndef AB_ORACLE_MP3_TRANSFORM_H
#define AB_ORACLE_MP3_TRANSFORM_H

#include "../../tests/mp3_transform_cases.h"

static uint32_t mp3_transform_bits(float number) {
    union {
        float number;
        uint32_t bits;
    } value = {.number = number};
    return value.bits;
}

static void mp3_transform_print(const float *values, size_t count) {
    uint32_t words[36];
    if (count > 36) {
        fail("MP3 transform word count exceeds its observer boundary\n");
    }
    for (size_t index = 0; index < count; ++index) {
        words[index] = mp3_transform_bits(values[index]);
    }
    print_words(words, count);
}

static void probe_mp3_transform(void) {
    int (*initialize)(void) = (int (*)(void))find_function("mpg123_init");
    void (*transform)(float *, const float *, float *, const float *, float *) =
#if defined(AB_ORACLE_MP3_DCT12_ONLY)
        (void (*)(float *, const float *, float *, const float *, float *))(
            find_function("INT123_do_layer3") - UINT32_C(0x8fc60c) +
            UINT32_C(0x8f847c));
#else
        (void (*)(float *, const float *, float *, const float *,
                  float *))find_function("INT123_dct36");
#endif
    if (initialize() != 0) {
        fail("Cannot initialize the original MP3 transform tables\n");
    }
#if defined(AB_ORACLE_MP3_DCT12_ONLY)
    semihost(4, "AB_ANDROID_MP3_DCT12_V1 1024\n");
#else
    semihost(4, "AB_ANDROID_MP3_DCT36_V2 1024\n");
    mp3_transform_print((const float *)find_symbol("INT123_COS9", 1), 9);
    mp3_transform_print((const float *)find_symbol("INT123_tfcos36", 1), 9);
    /* Resolve the two original PC-relative constant bases from this exact
     * function's LDR literals and ADD sites, using ELF disassembly offsets. */
    uintptr_t address = (uintptr_t)transform;
    uintptr_t first_base =
        address + UINT32_C(0x74) + *(const uint32_t *)(address + UINT32_C(0x608));
    uintptr_t second_base =
        address + UINT32_C(0x7c) + *(const uint32_t *)(address + UINT32_C(0x60c));
    float first[] = {*(const float *)first_base, *(const float *)(first_base + 16)};
    mp3_transform_print(first, 2);
    mp3_transform_print((const float *)(second_base + 968), 6);
#endif
    for (size_t sample = 0; sample < AB_TEST_MP3_TRANSFORM_CASES; ++sample) {
        float frequency[22], previous[22], next[22], window[36], output[580];
        float expected_previous[18];
        mp3_transform_case(sample, frequency, previous, window);
        for (size_t index = 0; index < 22; ++index) {
            next[index] = 1234.25f;
            if (index < 18) {
                expected_previous[index] = previous[index];
            } else {
                frequency[index] = 1234.25f;
                previous[index] = 1234.25f;
            }
        }
        for (size_t index = 0; index < 580; ++index) {
            output[index] = 1234.25f;
        }
        uint32_t meta = (uint32_t)sample;
        print_words(&meta, 1);
        mp3_transform_print(frequency, 18);
        mp3_transform_print(previous, 18);
        mp3_transform_print(window, 36);
        /* All five arrays are controlled first-party inputs/storage. The
         * original function runs normally; it writes 18 values at stride 32. */
        transform(frequency, previous, next, window, output);
        float samples[18];
        for (size_t index = 0; index < 18; ++index) {
            samples[index] = output[index * 32];
            if (mp3_transform_bits(previous[index]) !=
                mp3_transform_bits(expected_previous[index])) {
                fail("Original MP3 transform modified previous overlap input\n");
            }
        }
        for (size_t index = 0; index < 580; ++index) {
            if ((index % 32 != 0 || index >= 576) && output[index] != 1234.25f) {
                fail("Original MP3 transform exceeded its strided destination\n");
            }
        }
        for (size_t index = 18; index < 22; ++index) {
            if (frequency[index] != 1234.25f || previous[index] != 1234.25f ||
                next[index] != 1234.25f) {
                fail("Original MP3 transform exceeded its spectral/overlap bounds\n");
            }
        }
        mp3_transform_print(samples, 18);
        mp3_transform_print(next, 18);
    }
#if defined(AB_ORACLE_MP3_DCT12_ONLY)
    semihost(4, "AB MP3 dct12 reference complete\n");
#else
    semihost(4, "AB MP3 dct36 reference complete\n");
#endif
}

#endif
