#ifndef AB_ORACLE_BROADPHASE_PROBE_H
#define AB_ORACLE_BROADPHASE_PROBE_H

static unsigned broadphase_filter(void *object, const void *first, const void *second) {
    uint32_t *filter = object;
    const uint32_t *first_fixture = first;
    const uint32_t *second_fixture = second;
    if (filter[1] >= 2016 || !first_fixture[0] || first_fixture[0] > 64 ||
        !second_fixture[0] || second_fixture[0] > 64) {
        fail("Broadphase notification exceeded controlled fixture bounds\n");
    }
    filter[2 + filter[1] * 2] = first_fixture[0];
    filter[3 + filter[1] * 2] = second_fixture[0];
    ++filter[1];
    return 0;
}

static void print_broadphase(const uint32_t manager[20], const uint32_t *filter) {
    const uint32_t state[] = {
        manager[0], manager[2],  manager[3],  manager[4],  manager[6],  manager[7],
        manager[9], manager[10], manager[12], manager[13], manager[14], filter[1],
    };
    print_words(state, 12);
    if (manager[10]) {
        print_words((const uint32_t *)(uintptr_t)manager[8], manager[10]);
    }
    const uint32_t *pairs = (const uint32_t *)(uintptr_t)manager[11];
    for (size_t pair = 0; pair < manager[13]; ++pair) {
        print_words(pairs + pair * 2, 2);
    }
    for (size_t pair = 0; pair < filter[1]; ++pair) {
        print_words(filter + 2 + pair * 2, 2);
    }
}

static void probe_broadphase(void) {
    typedef void (*Construct)(void *);
    typedef int (*Create)(void *, const void *, void *);
    typedef void (*ProxyOperation)(void *, int);
    typedef void (*Move)(void *, int, const void *, const void *);
    typedef void (*Shift)(void *, const void *);
    Construct construct = (Construct)find_function("_ZN12b2BroadPhaseC1Ev");
    Construct close_phase = (Construct)find_function("_ZN12b2BroadPhaseD1Ev");
    Construct find_pairs =
        (Construct)find_function("_ZN16b2ContactManager15FindNewContactsEv");
    Create create = (Create)find_function("_ZN12b2BroadPhase11CreateProxyERK6b2AABBPv");
    ProxyOperation destroy =
        (ProxyOperation)find_function("_ZN12b2BroadPhase12DestroyProxyEi");
    ProxyOperation touch =
        (ProxyOperation)find_function("_ZN12b2BroadPhase10TouchProxyEi");
    Move move = (Move)find_function("_ZN12b2BroadPhase9MoveProxyEiRK6b2AABBRK6b2Vec2");
    Shift shift = (Shift)find_function("_ZN13b2DynamicTree11ShiftOriginERK6b2Vec2");
    uint32_t manager[20] = {0};
    uint32_t fixture_proxies[64][6] = {0}, fixtures[64][10] = {0};
    uint32_t bodies[64][40] = {0}, filter[4034] = {0};
    uint32_t filter_vtable[] = {0, 0, (uint32_t)(uintptr_t)broadphase_filter};
    filter[0] = (uint32_t)(uintptr_t)filter_vtable;
    construct(manager);
    manager[17] = (uint32_t)(uintptr_t)filter;
    int proxies[64];
    for (size_t slot = 0; slot < 64; ++slot) {
        proxies[slot] = -1;
        bodies[slot][0] = 2;
        fixtures[slot][0] = (uint32_t)slot + 1;
        fixtures[slot][2] = (uint32_t)(uintptr_t)bodies[slot];
        fixture_proxies[slot][4] = (uint32_t)(uintptr_t)fixtures[slot];
    }
    uint32_t state = UINT32_C(0x520b20ad);
    semihost(4, "AB_ANDROID_BROADPHASE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t operation = index == 0    ? 6
                             : index <= 64 ? 0
                             : index == 65 ? 4
                                           : random_word(&state) % 6;
        size_t slot = index && index <= 64 ? index - 1 : random_word(&state) % 64;
        if (operation == 0 && proxies[slot] != -1) {
            size_t attempt = 0;
            while (attempt++ < 64 && proxies[slot] != -1) {
                slot = (slot + 1) % 64;
            }
            if (proxies[slot] != -1) {
                operation = 3;
            }
        } else if (operation >= 1 && operation <= 3 && proxies[slot] == -1) {
            operation = 0;
        }
        uint32_t input[8] = {operation, (uint32_t)slot};
        if (index <= 65 || index % 2 == 0) {
            input[4] = UINT32_C(0x3f800000);
            input[5] = UINT32_C(0x3f800000);
        } else {
            input[2] = random_float(&state, 118, 12, 1);
            input[3] = random_float(&state, 118, 12, 1);
            input[4] = add_float(input[2], random_float(&state, 118, 10, 0));
            input[5] = add_float(input[3], random_float(&state, 118, 10, 0));
        }
        input[6] = random_float(&state, 115, 13, 1);
        input[7] = random_float(&state, 115, 13, 1);
        print_words(input, 8);
        filter[1] = 0;
        uint32_t result = 0;
        if (operation == 0) {
            proxies[slot] = create(manager, input + 2, fixture_proxies[slot]);
            result = (uint32_t)proxies[slot];
        } else if (operation == 1) {
            move(manager, proxies[slot], input + 2, input + 6);
        } else if (operation == 2) {
            destroy(manager, proxies[slot]);
            proxies[slot] = -1;
        } else if (operation == 3) {
            touch(manager, proxies[slot]);
        } else if (operation == 4) {
            find_pairs(manager);
        } else if (operation == 5) {
            shift(manager, input + 6);
        }
        print_words(&result, 1);
        print_broadphase(manager, filter);
    }
    close_phase(manager);
    semihost(4, "AB broadphase reference complete\n");
}

#endif
