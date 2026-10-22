#ifndef AB_TEST_SLING_START_H
#define AB_TEST_SLING_START_H

static bool sling_start_edges(void) {
    AbSlingTransferStart start = {
        .position = {1, 2}, .target = {3, 4}, .saved_y = NAN, .rotation = -0.5f};
    AbSlingTransferInput result;
    CHECK(ab_slingshot_transfer_begin(&start, &result));
    CHECK(result.start.x == 1 && result.start.y == 2 && result.time == 0 &&
          result.seconds == 0 && result.rotation == -0.5f &&
          to_bits(result.phase_angle) == UINT32_C(0x4016cbe4));
    AbSlingTransferInput previous = result;
    start.jumping = true;
    CHECK(!ab_slingshot_transfer_begin(&start, &result));
    CHECK(memcmp(&previous, &result, sizeof(result)) == 0);
    start.saved_y = 7;
    CHECK(ab_slingshot_transfer_begin(&start, &result));
    CHECK(result.start.y == 7 && result.height == -3.0f * 1.33f);
    CHECK(!ab_slingshot_transfer_begin(NULL, &result));
    CHECK(!ab_slingshot_transfer_begin(&start, NULL));
    return true;
}

static bool sling_start_native_checks(FILE *file) {
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[7], expected[10];
        CHECK(read_words(file, data, 7) && data[6] <= 1);
        AbSlingTransferStart start = {
            .position = {from_bits(data[0]), from_bits(data[1])},
            .target = {from_bits(data[2]), from_bits(data[3])},
            .saved_y = from_bits(data[4]),
            .rotation = from_bits(data[5]),
            .jumping = data[6] != 0};
        AbSlingTransferInput result;
        CHECK(ab_slingshot_transfer_begin(&start, &result));
        uint32_t actual[] = {to_bits(result.start.x),
                             to_bits(result.start.y),
                             to_bits(result.height),
                             to_bits(result.phase_angle),
                             to_bits(result.time),
                             data[4],
                             1,
                             0,
                             UINT32_C(0x461c4000),
                             to_bits(result.start.y)};
        CHECK(read_words(file, expected, 10));
        for (size_t field = 0; field < 10; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Sling initialization %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        uint32_t count;
        CHECK(read_words(file, &count, 1) && count == 2);
        AbSlingTransferFrame frame;
        CHECK(ab_slingshot_transfer_frame(&result, &frame));
        const uint32_t events[2][3] = {
            {1, to_bits(frame.position.x), to_bits(frame.position.y)},
            {2, to_bits(frame.rotation), 0},
        };
        for (size_t event = 0; event < 2; ++event) {
            CHECK(read_words(file, expected, 3));
            CHECK(memcmp(events[event], expected, sizeof(events[event])) == 0);
        }
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB sling start reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original-Lua transfer initializations match, including saved Y, "
         "reset phase, height, jump timer and 8192 transform calls.");
    return true;
}

#endif
