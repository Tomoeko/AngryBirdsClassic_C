#ifndef AB_TEST_SLING_TRANSFER_H
#define AB_TEST_SLING_TRANSFER_H

static bool transfer_edges(void) {
    AbSlingTransferInput input = {
        .start = {1, 2}, .target = {3, 4}, .height = 5, .time = 0.5f, .rotation = 0.5f};
    AbSlingTransferFrame result;
    CHECK(ab_slingshot_transfer_frame(&input, &result));
    CHECK(result.position.x == 2 && result.position.y == 2 && result.rotation == 0.5f &&
          result.next_time == 0.5f && !result.complete);
    input.time = 1;
    input.seconds = 0.5f;
    CHECK(ab_slingshot_transfer_frame(&input, &result));
    CHECK(result.position.x == 4 && result.position.y == 2 && result.next_time == 0 &&
          result.complete);
    AbSlingTransferFrame previous = result;
    input.phase_angle = FLT_MAX;
    CHECK(!ab_slingshot_transfer_frame(&input, &result));
    CHECK(memcmp(&previous, &result, sizeof(result)) == 0);
    CHECK(!ab_slingshot_transfer_frame(NULL, &result));
    CHECK(!ab_slingshot_transfer_frame(&input, NULL));
    return true;
}

static bool transfer_native_checks(FILE *file) {
    size_t completions = 0, calls = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[9], expected[5];
        CHECK(read_words(file, data, 9));
        AbSlingTransferInput input = {
            .start = {from_bits(data[0]), from_bits(data[1])},
            .target = {from_bits(data[2]), from_bits(data[3])},
            .height = from_bits(data[4]),
            .phase_angle = from_bits(data[5]),
            .time = from_bits(data[6]),
            .rotation = from_bits(data[7]),
            .seconds = from_bits(data[8])};
        AbSlingTransferFrame result;
        CHECK(ab_slingshot_transfer_frame(&input, &result));
        uint32_t summary[] = {to_bits(result.next_time), result.complete,
                              !result.complete, result.complete ? 0u : 4u,
                              result.complete ? UINT32_C(0x3f800000) : 0};
        CHECK(read_words(file, expected, 5));
        if (memcmp(summary, expected, sizeof(summary)) != 0) {
            fprintf(stderr, "Sling transfer %zu completion state differs\n", item);
            return false;
        }
        uint32_t count;
        CHECK(read_words(file, &count, 1));
        CHECK(count == (result.complete ? 5u : 2u));
        for (uint32_t event = 0; event < count; ++event) {
            uint32_t actual[3] = {0};
            if (event == 0) {
                actual[0] = 1;
                actual[1] = to_bits(result.position.x);
                actual[2] = to_bits(result.position.y);
            } else if (event == 1) {
                actual[0] = 2;
                actual[1] = to_bits(result.rotation);
            } else if (event == 2) {
                actual[0] = 1;
                actual[1] = to_bits(input.target.x - 0.1f);
                actual[2] = to_bits(input.target.y - 0.1f);
            } else {
                actual[0] = event == 3 ? 3u : 2u;
            }
            CHECK(read_words(file, expected, 3));
            for (size_t field = 0; field < 3; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Sling transfer %zu event %u field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            item, event, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
        completions += result.complete;
        calls += count;
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB sling transfer reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original-Lua sling transfers match: %zu completion boundaries, "
           "%zu ordered position/rotation/type calls.\n",
           completions, calls);
    return true;
}

#endif
