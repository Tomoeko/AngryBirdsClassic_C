#ifndef AB_ORACLE_BODY_SETUP_PROBE_H
#define AB_ORACLE_BODY_SETUP_PROBE_H

typedef unsigned (*OracleBodyEligibility)(const void *, const void *);
typedef unsigned (*OracleFixtureFilter)(void *, const void *, const void *);

static void probe_body_filters(OracleBodyEligibility body_eligibility,
                               OracleFixtureFilter fixture_filter, uint32_t *state,
                               size_t index) {
    uint32_t input[12] = {(uint32_t)(index % 3), (uint32_t)((index / 3) % 3),
                          (uint32_t)((index / 9) % 5), 1};
    uint32_t first_body[40] = {0}, second_body[40] = {0};
    uint32_t edges[4][4] = {0}, joints[4][16] = {0};
    first_body[0] = input[0];
    second_body[0] = input[1];
    for (size_t rule = 0; rule < input[2]; ++rule) {
        input[4 + rule * 2] = random_word(state) % 3 != 0;
        input[5 + rule * 2] = random_word(state) % 2 ? 0x80u : 0;
        edges[rule][0] = input[4 + rule * 2] ? (uint32_t)(uintptr_t)second_body : 0;
        edges[rule][1] = (uint32_t)(uintptr_t)joints[rule];
        edges[rule][3] = rule + 1 < input[2] ? (uint32_t)(uintptr_t)edges[rule + 1] : 0;
        ((uint8_t *)joints[rule])[0x3d] = (uint8_t)input[5 + rule * 2];
    }
    first_body[27] = input[2] ? (uint32_t)(uintptr_t)edges : 0;
    print_words(input, 4 + input[2] * 2);
    uint32_t allowed = body_eligibility(first_body, second_body) ? 1u : 0u;
    print_words(&allowed, 1);
    const uint32_t groups[] = {0, 1, 0xffff, 2, 0xfffe, 0x7fff, 0x8000};
    uint32_t filters[6];
    uint16_t fixtures[2][20] = {0};
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        filters[endpoint * 3] = random_word(state) & 0xffff;
        filters[endpoint * 3 + 1] = random_word(state) & 0xffff;
        filters[endpoint * 3 + 2] = groups[(index + endpoint * (index % 3)) % 7];
        if (index % 8 == 0) {
            filters[endpoint * 3] = 0;
            filters[endpoint * 3 + 1] = 0;
        }
        for (size_t field = 0; field < 3; ++field) {
            fixtures[endpoint][16 + field] = (uint16_t)filters[endpoint * 3 + field];
        }
    }
    print_words(filters, 6);
    allowed = fixture_filter(NULL, fixtures[0], fixtures[1]) ? 1u : 0u;
    print_words(&allowed, 1);
}

static void probe_body_setup(void) {
    typedef void (*ConstructBody)(void *, const void *, void *);
    ConstructBody construct =
        (ConstructBody)find_function("_ZN6b2BodyC1EPK9b2BodyDefP7b2World");
    OracleBodyEligibility eligibility =
        (OracleBodyEligibility)find_function("_ZNK6b2Body13ShouldCollideEPKS_");
    OracleFixtureFilter filter = (OracleFixtureFilter)find_function(
        "_ZN15b2ContactFilter13ShouldCollideEP9b2FixtureS1_");
    const uint32_t special_angles[] = {
        0, 0x80000000, 1, 0x80000001, 0x3fc90fdb, 0xbfc90fdb, 0x4dc90fda, 0xcdc90fda,
    };
    uint32_t state = UINT32_C(0x520b0d1e);
    semihost(4, "AB_ANDROID_BODY_SETUP_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t definition[13], body[64];
        definition[0] = (uint32_t)(index % 3);
        for (size_t field = 1; field < 9; ++field) {
            definition[field] = random_float(&state, 115, 20, 1);
        }
        definition[3] =
            index < 8 ? special_angles[index] : random_float(&state, 115, 25, 1);
        definition[7] = index % 5 ? random_float(&state, 118, 10, 0) : 0;
        definition[8] = index % 7 ? random_float(&state, 118, 10, 0) : 0x80000000;
        definition[9] = 0;
        for (size_t flag = 0; flag < 4; ++flag) {
            if ((index / 3) & ((size_t)1 << flag)) {
                definition[9] |= (index % 2 ? 1u : 0xffu) << (flag * 8);
            }
        }
        definition[10] = index % 32 < 16 ? 0xffu : 0;
        definition[11] = random_word(&state);
        definition[12] = random_float(&state, 120, 12, 1);
        for (size_t field = 0; field < 64; ++field) {
            body[field] = random_word(&state);
        }
        print_words(definition, 13);
        print_words(body, 64);
        construct(body, definition, (void *)(uintptr_t)UINT32_C(0x520abc00));
        print_words(body, 64);
        probe_body_filters(eligibility, filter, &state, index);
    }
    semihost(4, "AB body setup reference complete\n");
}

#endif
