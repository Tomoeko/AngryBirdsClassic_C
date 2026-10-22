#ifndef AB_ORACLE_DISTANCE_PROBE_H
#define AB_ORACLE_DISTANCE_PROBE_H

static void probe_distance(void) {
    typedef void (*Distance)(void *, void *, const void *);
    typedef unsigned (*Overlap)(const void *, int, const void *, int, const void *,
                                const void *);
    Distance distance = (Distance)find_function(
        "_Z10b2DistanceP16b2DistanceOutputP14b2SimplexCachePK15b2DistanceInput");
    Overlap overlap =
        (Overlap)find_function("_Z13b2TestOverlapPK7b2ShapeiS1_iRK11b2TransformS4_");
    uint32_t state = UINT32_C(0x520d157a);
    uint32_t cache[3] = {0, 0xa5a50000, 0x5a5a5a5a};
    const uint32_t rotations[][2] = {{0, 0x3f800000},
                                     {0x3f800000, 0},
                                     {0x3f19999a, 0x3f4ccccd},
                                     {0xbf3504f3, 0x3f3504f3}};
    semihost(4, "AB_ANDROID_DISTANCE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[45] = {(uint32_t)(1 + index % 8),
                              (uint32_t)(1 + (index / 8) % 8),
                              random_float(&state, 116, 10, 0),
                              random_float(&state, 116, 10, 0), (uint32_t)(index % 2)};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            input[5 + endpoint * 4] = random_float(&state, 116, 15, 1);
            input[6 + endpoint * 4] = random_float(&state, 116, 15, 1);
            size_t rotation = random_word(&state) % 4;
            input[7 + endpoint * 4] = rotations[rotation][0];
            input[8 + endpoint * 4] = rotations[rotation][1];
            for (size_t vertex = 0; vertex < input[endpoint]; ++vertex) {
                input[13 + endpoint * 16 + vertex * 2] =
                    random_float(&state, 118, 15, 1);
                input[14 + endpoint * 16 + vertex * 2] =
                    random_float(&state, 118, 15, 1);
            }
        }
        if (index < 64) {
            const uint32_t edges[] = {0,          0x80000000, 1,          0x80000001,
                                      0x007fffff, 0x00800000, 0x33ffffff, 0x34000000,
                                      0x34000001, 0x35a00000, 0x359fffff, 0x35a00001,
                                      0x3f800000, 0x4dc90fda, 0x40000000, 0x3f800001};
            for (size_t field = 0; field < 45; ++field) {
                input[field] = 0;
            }
            input[0] = input[1] = 1;
            input[3] = (const uint32_t[]){0, 1, 0x33800000, 0x3f000000}[index / 16];
            input[4] = (uint32_t)(index % 2);
            input[8] = input[12] = 0x3f800000;
            input[29] = edges[index % 16];
        }
        if (index < 64 || index % 16 == 0) {
            ((uint16_t *)cache)[2] = 0;
        }
        if (index % 17 == 0) {
            cache[0] = random_float(&state, 110, 30, 0);
        }
        for (size_t vertex = 0; vertex < 3; ++vertex) {
            ((uint8_t *)cache)[6 + vertex] %= (uint8_t)input[0];
            ((uint8_t *)cache)[9 + vertex] %= (uint8_t)input[1];
        }
        uint32_t native_input[23] = {0}, output[6] = {0};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            native_input[endpoint * 7 + 4] =
                (uint32_t)(uintptr_t)(input + 13 + endpoint * 16);
            native_input[endpoint * 7 + 5] = input[endpoint];
            native_input[endpoint * 7 + 6] = input[2 + endpoint];
            for (size_t field = 0; field < 4; ++field) {
                native_input[14 + endpoint * 4 + field] =
                    input[5 + endpoint * 4 + field];
            }
        }
        native_input[22] = input[4];
        print_words(input, 45);
        print_words(cache, 3);
        uint32_t shapes[2][38] = {0};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            shapes[endpoint][1] = input[endpoint] == 1 ? 0u : 2u;
            shapes[endpoint][2] = input[2 + endpoint];
            size_t vertex_offset = input[endpoint] == 1 ? 3 : 5;
            for (size_t field = 0; field < input[endpoint] * 2; ++field) {
                shapes[endpoint][vertex_offset + field] =
                    input[13 + endpoint * 16 + field];
            }
            shapes[endpoint][37] = input[endpoint];
        }
        uint32_t overlapping =
            overlap(shapes[0], 0, shapes[1], 0, input + 5, input + 9) ? 1u : 0u;
        print_words(&overlapping, 1);
        distance(output, cache, native_input);
        print_words(output, 6);
        print_words(cache, 3);
    }
    semihost(4, "AB distance reference complete\n");
}

#endif
