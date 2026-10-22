#ifndef AB_ORACLE_COMPOSITE_BOUNDS_H
#define AB_ORACLE_COMPOSITE_BOUNDS_H

static void probe_composite_bounds(void) {
    void (*calculate)(void *) =
        (void (*)(void *))find_function("_ZN4game11CompoSprite15calculateBoundsEv");
    uint32_t composite[15] = {0}, entries[8][12] = {{0}}, sprites[8][16] = {{0}};
    uint32_t pointers[8], counters[8][2];
    for (size_t index = 0; index < 8; ++index) {
        pointers[index] = (uint32_t)(uintptr_t)entries[index];
        entries[index][1] = (uint32_t)(uintptr_t)counters[index];
        entries[index][3] = (uint32_t)(uintptr_t)sprites[index];
        counters[index][0] = 1000;
        counters[index][1] = 1;
    }
    composite[2] = (uint32_t)(uintptr_t)pointers;
    uint32_t random = UINT32_C(0x520b0a0d);
    semihost(4, "AB_ANDROID_COMPOSITE_BOUNDS_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t count = (uint32_t)(item % 9);
        composite[3] = (uint32_t)(uintptr_t)(pointers + count);
        print_words(&count, 1);
        for (size_t index = 0; index < count; ++index) {
            uint32_t input[12];
            input[0] = random_word(&random) % 2049;
            input[1] = random_word(&random) % 2049;
            for (size_t field = 2; field < 4; ++field) {
                input[field] = (uint32_t)((int32_t)(random_word(&random) % 513) - 256);
            }
            for (size_t field = 4; field < 11; ++field) {
                FloatBits value = {
                    .number =
                        (float)((int32_t)(random_word(&random) % 513) - 256) / 16};
                input[field] = value.bits;
            }
            if (item % 5 == 0) {
                input[6] = input[7] = input[8] = input[9] = UINT32_C(0x3f800000);
                input[10] = 0;
            }
            input[11] = (uint32_t)((item + index) % 4 != 0);
            sprites[index][5] = input[0] | (input[1] << 16);
            for (size_t axis = 0; axis < 2; ++axis) {
                FloatBits pivot = {.number = (float)(int32_t)input[2 + axis]};
                sprites[index][6 + axis] = pivot.bits;
            }
            for (size_t field = 4; field < 12; ++field) {
                entries[index][field] = input[field];
            }
            print_words(input, 12);
        }
        calculate(composite);
        print_words(composite + 11, 4);
        for (size_t index = 0; index < 8; ++index) {
            if (counters[index][0] != 1000 || counters[index][1] != 1) {
                fail("Native composite bounds changed prepared reference counts\n");
            }
        }
    }
    semihost(4, "AB composite bounds reference complete\n");
}

#endif
