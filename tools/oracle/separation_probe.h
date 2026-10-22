#ifndef AB_ORACLE_SEPARATION_PROBE_H
#define AB_ORACLE_SEPARATION_PROBE_H

static void probe_separation(void) {
    typedef float (*Initialize)(void *, const void *, const void *, const void *,
                                const void *, const void *, float);
    typedef float (*Find)(const void *, int *, int *, float);
    typedef float (*Evaluate)(const void *, int, int, float);
    Initialize initialize =
        (Initialize)find_function("_ZN20b2SeparationFunction10InitializeEPK14b2SimplexC"
                                  "achePK15b2DistanceProxyRK7b2SweepS5_S8_f");
    Find find =
        (Find)find_function("_ZNK20b2SeparationFunction17FindMinSeparationEPiS0_f");
    Evaluate evaluate =
        (Evaluate)find_function("_ZNK20b2SeparationFunction8EvaluateEiif");
    const uint32_t fractions[] = {0, 0x80000000, 0x3f000000, 0x3f800000, 0x3eaaaaab};
    const uint32_t boundaries[] = {0,          0x80000000, 1,          0x80000001,
                                   0x007fffff, 0x00800000, 0x33ffffff, 0x34000000,
                                   0x34000001, 0x3f800000};
    uint32_t random = UINT32_C(0x5205e9a2);
    semihost(4, "AB_ANDROID_SEPARATION_V1 4096 5\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[58] = {(uint32_t)(1 + index % 8),
                              (uint32_t)(1 + (index / 8) % 8)};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            input[2 + endpoint] = random_float(&random, 115, 10, 0);
            for (size_t vertex = 0; vertex < input[endpoint]; ++vertex) {
                input[4 + endpoint * 16 + vertex * 2] =
                    random_float(&random, 115, 20, 1);
                input[5 + endpoint * 16 + vertex * 2] =
                    random_float(&random, 115, 20, 1);
            }
            for (size_t field = 0; field < 9; ++field) {
                input[36 + endpoint * 9 + field] = random_float(&random, 115, 23, 1);
            }
            input[44 + endpoint * 9] = fractions[(index + endpoint) % 5];
        }
        if (index < 64) {
            for (size_t field = 4; field < 54; ++field) {
                input[field] = 0;
            }
            for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
                for (size_t vertex = 1; vertex < input[endpoint]; ++vertex) {
                    input[4 + endpoint * 16 + vertex * 2] = boundaries[index % 10];
                }
            }
            input[47] = input[49] = 0x40000000;
        }
        uint32_t *cache = input + 54;
        cache[0] = random_float(&random, 115, 15, 0);
        cache[1] = random_word(&random);
        cache[2] = random_word(&random);
        ((uint16_t *)cache)[2] = (uint16_t)(index % 3 == 0 ? 1 : 2);
        uint8_t *cache_bytes = (uint8_t *)cache;
        cache_bytes[6] = (uint8_t)(random_word(&random) % input[0]);
        cache_bytes[7] = index % 3 == 1 ? cache_bytes[6]
                                        : (uint8_t)((cache_bytes[6] + 1) % input[0]);
        cache_bytes[9] = (uint8_t)(random_word(&random) % input[1]);
        cache_bytes[10] = (uint8_t)((cache_bytes[9] + 1) % input[1]);
        input[57] = fractions[index % 5];
        uint32_t proxies[2][7] = {0};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            proxies[endpoint][4] = (uint32_t)(uintptr_t)(input + 4 + endpoint * 16);
            proxies[endpoint][5] = input[endpoint];
            proxies[endpoint][6] = input[2 + endpoint];
        }
        uint32_t state[25], before_queries[25], normalized[25];
        for (size_t field = 0; field < 25; ++field) {
            state[field] = random_word(&random);
        }
        print_words(input, 58);
        print_words(state, 25);
        FloatBits fraction = {.bits = input[57]}, result = {0};
        result.number = initialize(state, cache, proxies[0], input + 36, proxies[1],
                                   input + 45, fraction.number);
        for (size_t field = 0; field < 25; ++field) {
            before_queries[field] = state[field];
            normalized[field] = state[field];
        }
        normalized[0] = 0x52000001;
        normalized[1] = 0x52000002;
        print_words(normalized, 25);
        print_words(&result.bits, 1);
        for (size_t query = 0; query < 5; ++query) {
            uint32_t controls[] = {fractions[query], random_word(&random) % input[0],
                                   random_word(&random) % input[1]};
            fraction.bits = controls[0];
            int first = 0, second = 0;
            result.number = find(state, &first, &second, fraction.number);
            uint32_t output[] = {(uint32_t)first, (uint32_t)second, result.bits, 0, 0};
            result.number = evaluate(state, first, second, fraction.number);
            output[3] = result.bits;
            result.number =
                evaluate(state, (int)controls[1], (int)controls[2], fraction.number);
            output[4] = result.bits;
            print_words(controls, 3);
            print_words(output, 5);
        }
        for (size_t field = 0; field < 25; ++field) {
            if (state[field] != before_queries[field]) {
                fail("Native separation const query changed state\n");
            }
        }
    }
    semihost(4, "AB separation reference complete\n");
}

#endif
