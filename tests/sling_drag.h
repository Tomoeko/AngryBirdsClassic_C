#ifndef AB_TEST_SLING_DRAG_H
#define AB_TEST_SLING_DRAG_H

static bool sling_drag_edges(void) {
    AbSlingDragInput input = {
        .rest = {10, 20}, .displacement = {1, 0}, .maximum_length = 2, .wobble = 5};
    AbSlingDragFrame frame;
    CHECK(ab_slingshot_drag(&input, &frame));
    CHECK(frame.rubber_band.x == 9 && frame.rubber_band.y == 20 &&
          frame.distance == 1 && frame.stretch_factor == 0.5f && frame.angle == 0 &&
          frame.wobble == 5 && frame.selection_played && frame.event_count == 3 &&
          frame.events[0] == AB_SLING_DRAG_QUERY_STRETCH &&
          frame.events[1] == AB_SLING_DRAG_PLAY_STRETCH &&
          frame.events[2] == AB_SLING_DRAG_PLAY_SELECTION);
    input.selection_played = true;
    input.displacement = (AbVec2){4, 0};
    CHECK(ab_slingshot_drag(&input, &frame));
    CHECK(frame.rubber_band.x == 8 && frame.rubber_band.y == 20 &&
          frame.distance == 2 && frame.wobble == 0 && !frame.event_count);
    input.displacement = (AbVec2){0, -4};
    CHECK(ab_slingshot_drag(&input, &frame));
    CHECK(frame.rubber_band.x == 10 && frame.rubber_band.y == 20.5f &&
          frame.distance == 0.5f && to_bits(frame.angle) == UINT32_C(0xbfc90fdb));
    input.displacement = (AbVec2){-0.0f, 0};
    CHECK(ab_slingshot_drag(&input, &frame));
    CHECK(to_bits(frame.angle) == 0 && frame.distance == 0);
    unsigned char previous[sizeof(frame)];
    memcpy(previous, &frame, sizeof(frame));
    input.maximum_length = 0;
    CHECK(!ab_slingshot_drag(&input, &frame));
    CHECK(memcmp(previous, &frame, sizeof(frame)) == 0);
    CHECK(!ab_slingshot_drag(NULL, &frame));
    CHECK(!ab_slingshot_drag(&input, NULL));
    AbSlingStretch measured = {3, 4}, saved = measured;
    CHECK(!ab_slingshot_measure_stretch((AbVec2){0}, (AbVec2){0}, 0, &measured));
    CHECK(memcmp(&measured, &saved, sizeof(measured)) == 0);
    CHECK(!ab_slingshot_measure_stretch((AbVec2){NAN, 0}, (AbVec2){0}, 1, &measured));
    CHECK(!ab_slingshot_measure_stretch((AbVec2){0}, (AbVec2){0}, 1, NULL));
    return true;
}

static bool sling_drag_native_checks(FILE *file) {
    size_t event_count = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[9], expected[8];
        CHECK(read_words(file, input, 9) && input[7] <= 1 && input[8] <= 1);
        CHECK(read_words(file, expected, 8) && expected[7] <= 3);
        AbSlingDragInput controls = {
            .rest = {from_bits(input[0]), from_bits(input[1])},
            .displacement = {from_bits(input[2]), from_bits(input[3])},
            .maximum_length = from_bits(input[4]),
            .previous_distance = from_bits(input[5]),
            .wobble = from_bits(input[6]),
            .selection_played = input[7] != 0,
            .stretch_audio_playing = input[8] != 0};
        AbSlingDragFrame frame;
        CHECK(ab_slingshot_drag(&controls, &frame));
        uint32_t actual[] = {to_bits(frame.rubber_band.x),
                             to_bits(frame.rubber_band.y),
                             to_bits(frame.angle),
                             to_bits(frame.stretch_factor),
                             to_bits(frame.distance),
                             to_bits(frame.wobble),
                             frame.selection_played ? 1u : 0u,
                             frame.event_count};
        for (size_t field = 0; field < 8; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native drag %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        for (size_t event = 0; event < frame.event_count; ++event) {
            uint32_t kind;
            CHECK(read_words(file, &kind, 1) && kind == (uint32_t)frame.events[event]);
        }
        event_count += frame.event_count;
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB sling drag reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf(
        "All 4096 original-Lua drag states and %zu ordered audio boundaries match.\n",
        event_count);
    return true;
}

#endif
