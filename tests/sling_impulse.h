#ifndef AB_TEST_SLING_IMPULSE_H
#define AB_TEST_SLING_IMPULSE_H

static bool sling_impulse_edges(void) {
    AbSlingLaunchInput input = {.rubber_band = {3, 4},
                                .stretch = 2,
                                .launch_force = 10,
                                .physics_scale = 0.5f,
                                .bird_mass = 2,
                                .slingshot_power = 1,
                                .bird_present = true};
    AbVec2 output;
    CHECK(ab_slingshot_launch_impulse(&input, NULL, &output));
    CHECK(output.x == 12 && output.y == 16);
    AbVec2 override = {0, -2};
    CHECK(ab_slingshot_launch_impulse(&input, &override, &output));
    CHECK(to_bits(output.x) == UINT32_C(0x80000000) && output.y == 20);
    AbVec2 previous = output;
    input.bird_present = false;
    CHECK(!ab_slingshot_launch_impulse(&input, NULL, &output));
    CHECK(memcmp(&previous, &output, sizeof(output)) == 0);
    input.bird_present = true;
    override = (AbVec2){0};
    CHECK(!ab_slingshot_launch_impulse(&input, &override, &output));
    CHECK(!ab_slingshot_launch_impulse(NULL, NULL, &output));
    CHECK(!ab_slingshot_launch_impulse(&input, NULL, NULL));
    return true;
}

static bool sling_impulse_native_checks(FILE *file, bool routing) {
    size_t backwards_count = 0, releases = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[18] = {0}, expected[10];
        CHECK(read_words(file, data, routing ? 18 : 14) && data[13] <= 1 &&
              (data[10] & 1));
        CHECK(read_words(file, expected, routing ? 10 : 6) && expected[4] == 1 &&
              expected[5] == 1);
        AbSlingLaunchInput input = {
            .rest = {from_bits(data[0]), from_bits(data[1])},
            .rubber_band = {from_bits(data[2]), from_bits(data[3])},
            .stretch = from_bits(data[4]),
            .launch_force = from_bits(data[data[10] & 2 ? 5 : 6]),
            .physics_scale = from_bits(data[7]),
            .bird_mass = from_bits(data[8]),
            .slingshot_power = from_bits(data[9]),
            .bird_present = true};
        AbVec2 vector = {from_bits(data[11]), from_bits(data[12])}, impulse;
        CHECK(ab_slingshot_launch_impulse(&input, data[13] ? &vector : NULL, &impulse));
        if (routing) {
            CHECK(data[14] <= 4 && data[15] <= 4);
            float horizontal = data[13] ? vector.x : input.rest.x - input.rubber_band.x;
            bool backwards =
                ab_slingshot_counts_backwards(horizontal, data[14] == 2, data[15] >= 2);
            bool launch =
                ab_slingshot_release_ready(from_bits(data[16]), from_bits(data[17]));
            CHECK(expected[6] == (backwards ? 1u : 0u) &&
                  expected[7] == (backwards ? 0x21u : 1u) &&
                  expected[8] == (launch ? 1u : 0u) &&
                  expected[9] == (launch ? 0u : 1u));
            backwards_count += backwards;
            releases += launch;
        }
        uint32_t actual[] = {to_bits(input.rubber_band.x), to_bits(input.rubber_band.y),
                             to_bits(impulse.x), to_bits(impulse.y)};
        for (size_t field = 0; field < 4; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native launch impulse %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, routing ? "AB sling routing reference complete\n"
                                 : "AB sling impulse reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original-Lua actual launch impulse boundaries match.");
    if (routing) {
        printf("Also matched %zu backwards increments and %zu launch/%zu cancel "
               "releases, including equality, infinities and NaNs.\n",
               backwards_count, releases, 4096 - releases);
    }
    return true;
}

#endif
