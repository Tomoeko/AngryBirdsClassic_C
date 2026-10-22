#ifndef AB_ORACLE_FIXTURE_PROBE_H
#define AB_ORACLE_FIXTURE_PROBE_H

static int fixture_child_count(const void *shape) {
    (void)shape;
    return 1;
}

static void print_fixture_state(const uint32_t phase[15], const uint32_t fixture[10],
                                const uint32_t proxy[7]) {
    uint32_t output[10] = {fixture[7], proxy[0], proxy[1],
                           proxy[2],   proxy[3], proxy[6]};
    if (fixture[7]) {
        const uint32_t *node = (const uint32_t *)(uintptr_t)phase[1] + proxy[6] * 9;
        for (size_t field = 0; field < 4; ++field) {
            output[6 + field] = node[field];
        }
    }
    print_words(output, 10);
    uint32_t move_hash = UINT32_C(2166136261);
    const uint32_t *moves = (const uint32_t *)(uintptr_t)phase[8];
    for (size_t move = 0; move < phase[10]; ++move) {
        move_hash = (move_hash ^ moves[move]) * UINT32_C(16777619);
    }
    const uint32_t summary[] = {phase[0],  phase[2],  phase[3], phase[4],
                                phase[6],  phase[7],  phase[9], phase[10],
                                phase[12], phase[13], move_hash};
    print_words(summary, 11);
}

static void probe_fixtures(void) {
    typedef void (*Construct)(void *);
    typedef void (*ProxyOperation)(void *, void *);
    typedef void (*Create)(void *, void *, const void *);
    typedef void (*Synchronize)(void *, void *, const void *, const void *);
    typedef void (*SetBox)(void *, float, float);
    Construct construct = (Construct)find_function("_ZN12b2BroadPhaseC1Ev");
    Construct close_phase = (Construct)find_function("_ZN12b2BroadPhaseD1Ev");
    Create create = (Create)find_function(
        "_ZN9b2Fixture13CreateProxiesEP12b2BroadPhaseRK11b2Transform");
    ProxyOperation destroy =
        (ProxyOperation)find_function("_ZN9b2Fixture14DestroyProxiesEP12b2BroadPhase");
    Synchronize synchronize = (Synchronize)find_function(
        "_ZN9b2Fixture11SynchronizeEP12b2BroadPhaseRK11b2TransformS4_");
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    uint32_t vtables[2][7] = {0};
    vtables[0][6] = (uint32_t)(uintptr_t)find_function(
        "_ZNK13b2CircleShape11ComputeAABBEP6b2AABBRK11b2Transformi");
    vtables[1][6] = (uint32_t)(uintptr_t)find_function(
        "_ZNK14b2PolygonShape11ComputeAABBEP6b2AABBRK11b2Transformi");
    vtables[0][3] = vtables[1][3] = (uint32_t)(uintptr_t)fixture_child_count;
    uint32_t phase[15] = {0}, fixtures[32][10] = {0}, proxies[32][7] = {0};
    uint32_t shapes[32][38] = {0}, shape_parameters[32][4] = {0};
    for (size_t slot = 0; slot < 32; ++slot) {
        fixtures[slot][3] = (uint32_t)(uintptr_t)shapes[slot];
        fixtures[slot][6] = (uint32_t)(uintptr_t)proxies[slot];
        proxies[slot][6] = UINT32_MAX;
    }
    construct(phase);
    uint32_t state = UINT32_C(0x520f17e5);
    const uint32_t rotations[][2] = {{0, 0x3f800000},
                                     {0x3f800000, 0},
                                     {0x3f19999a, 0x3f4ccccd},
                                     {0xbf3504f3, 0x3f3504f3}};
    semihost(4, "AB_ANDROID_FIXTURE_PROXIES_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t operation = index < 32 ? 0 : random_word(&state) % 3;
        size_t slot = index < 32 ? index : random_word(&state) % 32;
        if (operation == 0 && fixtures[slot][7]) {
            operation = 1;
        }
        if (operation == 0) {
            uint32_t kind = random_word(&state) % 2;
            shape_parameters[slot][0] = kind ? 2u : 0u;
            shape_parameters[slot][1] =
                index % 8 ? random_float(&state, 116, 10, 0) : 0;
            shape_parameters[slot][2] = random_float(&state, 118, 14, kind ? 0 : 1);
            shape_parameters[slot][3] = random_float(&state, 118, 14, kind ? 0 : 1);
            shapes[slot][0] = (uint32_t)(uintptr_t)vtables[kind];
            shapes[slot][1] = shape_parameters[slot][0];
            shapes[slot][2] = shape_parameters[slot][1];
            if (kind) {
                FloatBits width = {.bits = shape_parameters[slot][2]};
                FloatBits height = {.bits = shape_parameters[slot][3]};
                set_box(shapes[slot], width.number, height.number);
            } else {
                shapes[slot][3] = shape_parameters[slot][2];
                shapes[slot][4] = shape_parameters[slot][3];
            }
        }
        uint32_t input[14] = {operation, (uint32_t)slot};
        for (size_t field = 0; field < 4; ++field) {
            input[2 + field] = shape_parameters[slot][field];
        }
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            input[6 + endpoint * 4] = random_float(&state, 115, 20, 1);
            input[7 + endpoint * 4] = random_float(&state, 115, 20, 1);
            size_t rotation = random_word(&state) % 4;
            input[8 + endpoint * 4] = rotations[rotation][0];
            input[9 + endpoint * 4] = rotations[rotation][1];
        }
        if (index % 8 == 0) {
            for (size_t field = 0; field < 4; ++field) {
                input[10 + field] = input[6 + field];
            }
        }
        print_words(input, 14);
        if (operation == 0) {
            create(fixtures[slot], phase, input + 6);
        } else if (operation == 1) {
            synchronize(fixtures[slot], phase, input + 6, input + 10);
        } else {
            destroy(fixtures[slot], phase);
        }
        print_fixture_state(phase, fixtures[slot], proxies[slot]);
    }
    close_phase(phase);
    semihost(4, "AB fixture proxy reference complete\n");
}

#endif
