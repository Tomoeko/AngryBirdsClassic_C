#ifndef AB_ORACLE_IMPACT_PROBE_H
#define AB_ORACLE_IMPACT_PROBE_H

static uint32_t impact_number(float number) {
    FloatBits value = {.number = number};
    return value.bits;
}

static void prepare_impact_geometry(uint32_t input[55], uint32_t *random,
                                    size_t index) {
    const float vertices[][2] = {{1, 0},  {0.6f, 0.8f},   {0, 1},  {-0.6f, 0.8f},
                                 {-1, 0}, {-0.6f, -0.8f}, {0, -1}, {0.6f, -0.8f}};
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        input[endpoint] = (index + endpoint) % 4 < 2
                              ? 1u
                              : (uint32_t)(3 + (index + endpoint * 3) % 6);
        input[2 + endpoint] = input[endpoint] == 1 ? random_float(random, 124, 4, 0)
                                                   : impact_number(0.01f);
        FloatBits width = {.bits = random_float(random, 125, 3, 0)};
        FloatBits height = {.bits = random_float(random, 125, 3, 0)};
        for (size_t vertex = 0; vertex < input[endpoint]; ++vertex) {
            input[4 + endpoint * 16 + vertex * 2] = impact_number(
                input[endpoint] == 1 ? 0 : vertices[vertex][0] * width.number);
            input[5 + endpoint * 16 + vertex * 2] = impact_number(
                input[endpoint] == 1 ? 0 : vertices[vertex][1] * height.number);
        }
    }
}

static void prepare_impact_sweeps(uint32_t input[55], uint32_t *random, size_t index) {
    const uint32_t fractions[] = {0, 0x3e800000, 0x3f000000, 0x3f800000};
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        uint32_t *sweep = input + 36 + endpoint * 9;
        sweep[0] = random_float(random, 115, 10, 1);
        sweep[1] = random_float(random, 115, 10, 1);
        sweep[2] = random_float(random, 119, 14, 1);
        sweep[3] = random_float(random, 119, 14, 1);
        sweep[4] = add_float(sweep[2], random_float(random, 118, 16, 1));
        sweep[5] = add_float(sweep[3], random_float(random, 118, 16, 1));
        sweep[6] = random_float(random, 125, 6, 1);
        sweep[7] = add_float(sweep[6], random_float(random, 122, 7, 1));
        sweep[8] = fractions[(index + endpoint) % 4];
        if (index % 8 == 0) {
            sweep[4] = sweep[2];
            sweep[5] = sweep[3];
            sweep[7] = sweep[6];
        } else if (index % 8 <= 4) {
            for (size_t field = 0; field < 8; ++field) {
                sweep[field] = 0;
            }
            if (endpoint && index % 8 == 1) {
                sweep[2] = impact_number(4);
                sweep[4] = impact_number(-4);
            } else if (endpoint && index % 8 == 3) {
                sweep[2] = impact_number(4);
                sweep[4] = impact_number(8);
            } else if (endpoint && index % 8 == 4) {
                sweep[2] = impact_number(100);
                sweep[4] = impact_number(-100);
            }
        } else if (index % 8 == 6) {
            sweep[6] = impact_number(endpoint ? -0x1p22f : 0x1p20f);
            sweep[7] = add_float(sweep[6], random_float(random, 125, 4, 1));
        }
    }
    input[54] = index % 16 == 0 ? 0
                                : (const uint32_t[]){0x3f800000, 0x3e800000, 0x3f000000,
                                                     0x3f400000}[(index / 16) % 4];
    if (index < 64) {
        const uint32_t distances[] = {
            0,
            0x80000000,
            1,
            0x80000001,
            0x34000000,
            0x35a00000,
            impact_number(0.0375f),
            impact_number(0.05f),
            impact_number(0.0625f),
            impact_number(0.15f),
            impact_number(0.85f),
            impact_number(0.8625f),
            impact_number(1.85f),
            impact_number(1.8625f),
            impact_number(4),
            impact_number(100),
        };
        for (size_t field = 0; field < 55; ++field) {
            input[field] = 0;
        }
        input[0] = input[1] = 1;
        input[2] = input[3] =
            (const uint32_t[]){0, 0x3c23d70a, 0x3f000000, 0x3f800000}[index / 16];
        input[47] = input[49] = distances[index % 16];
        input[42] = input[43] = input[51] = input[52] = index % 2 ? 0x80000000 : 0;
        input[54] = 0x3f800000;
    }
}

static void probe_impact(void) {
    typedef void (*Impact)(void *, const void *);
    Impact impact =
        (Impact)find_function("_Z14b2TimeOfImpactP11b2TOIOutputPK10b2TOIInput");
    const uint32_t *iterations = (const uint32_t *)find_symbol("b2_toiIters", 1);
    const uint32_t *roots = (const uint32_t *)find_symbol("b2_toiRootIters", 1);
    uint32_t random = UINT32_C(0x520701cc);
    semihost(4, "AB_ANDROID_IMPACT_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[55] = {0}, native[33] = {0}, before[33];
        prepare_impact_geometry(input, &random, index);
        prepare_impact_sweeps(input, &random, index);
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            native[endpoint * 7 + 4] = (uint32_t)(uintptr_t)(input + 4 + endpoint * 16);
            native[endpoint * 7 + 5] = input[endpoint];
            native[endpoint * 7 + 6] = input[2 + endpoint];
        }
        for (size_t field = 0; field < 18; ++field) {
            native[14 + field] = input[36 + field];
        }
        native[32] = input[54];
        for (size_t field = 0; field < 33; ++field) {
            before[field] = native[field];
        }
        print_words(input, 55);
        uint32_t output[4] = {0}, previous_iterations = *iterations,
                 previous_roots = *roots;
        impact(output, native);
        output[2] = *iterations - previous_iterations;
        output[3] = *roots - previous_roots;
        print_words(output, 4);
        for (size_t field = 0; field < 33; ++field) {
            if (native[field] != before[field]) {
                fail("Native impact query modified input\n");
            }
        }
    }
    semihost(4, "AB impact reference complete\n");
}

#endif
