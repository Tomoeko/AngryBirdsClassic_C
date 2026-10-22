#ifndef AB_ORACLE_TREE_PROBE_H
#define AB_ORACLE_TREE_PROBE_H

static unsigned tree_query_callback(void *object, uintptr_t fixture,
                                    const void *proxy) {
    (void)proxy;
    uint32_t *callback = object;
    if (!fixture || fixture > 64 || callback[1] >= 64) {
        fail("Tree query callback exceeded controlled fixture bounds\n");
    }
    callback[3 + callback[1]++] = (uint32_t)fixture;
    return callback[1] < callback[2];
}

static void print_tree_state(const uint32_t tree[7], const uint32_t fixtures[64][5]) {
    const uint32_t state[] = {tree[0], tree[2], tree[3], tree[4], tree[5], tree[6]};
    print_words(state, 6);
    const uint32_t *nodes = (const uint32_t *)(uintptr_t)tree[1];
    for (size_t index = 0; index < tree[3]; ++index) {
        const uint32_t *node = nodes + index * 9;
        uint32_t record[9] = {0};
        record[5] = node[5];
        record[8] = node[8];
        if (node[8] != UINT32_MAX) {
            for (size_t field = 0; field < 4; ++field) {
                record[field] = node[field];
            }
            if (node[4]) {
                unsigned found = 0;
                for (size_t fixture = 0; fixture < 64; ++fixture) {
                    if (node[4] == (uint32_t)(uintptr_t)fixtures[fixture]) {
                        record[4] = (uint32_t)fixture + 1;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    fail("Tree node payload is outside the controlled fixture pool\n");
                }
            }
            record[6] = node[6];
            record[7] = node[7];
        }
        print_words(record, 9);
    }
}

static void probe_tree(void) {
    typedef void (*Construct)(void *);
    typedef int (*Create)(void *, const void *, void *);
    typedef void (*Destroy)(void *, int);
    typedef unsigned (*Move)(void *, int, const void *, const void *);
    typedef void (*Shift)(void *, const void *);
    typedef void (*Query)(const void *, void *, const void *);
    Construct construct = (Construct)find_function("_ZN13b2DynamicTreeC1Ev");
    Construct close_tree = (Construct)find_function("_ZN13b2DynamicTreeD1Ev");
    Create create =
        (Create)find_function("_ZN13b2DynamicTree11CreateProxyERK6b2AABBPv");
    Destroy destroy = (Destroy)find_function("_ZN13b2DynamicTree12DestroyProxyEi");
    Move move = (Move)find_function("_ZN13b2DynamicTree9MoveProxyEiRK6b2AABBRK6b2Vec2");
    Shift shift = (Shift)find_function("_ZN13b2DynamicTree11ShiftOriginERK6b2Vec2");
    Query query = (Query)find_function(
        "_ZNK13b2DynamicTree5QueryI19b2WorldQueryWrapperEEvPT_RK6b2AABB");
    uint32_t tree[7] = {0}, fixtures[64][5] = {0};
    int proxies[64];
    for (size_t slot = 0; slot < 64; ++slot) {
        proxies[slot] = -1;
        fixtures[slot][4] = (uint32_t)slot + 1;
    }
    construct(tree);
    uint32_t state = UINT32_C(0x5207aee1);
    semihost(4, "AB_ANDROID_TREE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t operation = index == 0 ? 4 : index <= 64 ? 0 : random_word(&state) % 4;
        size_t slot = index && index <= 64 ? index - 1 : random_word(&state) % 64;
        if (operation == 0 && proxies[slot] != -1) {
            size_t attempt = 0;
            while (attempt++ < 64 && proxies[slot] != -1) {
                slot = (slot + 1) % 64;
            }
            if (proxies[slot] != -1) {
                operation = 1;
            }
        } else if ((operation == 1 || operation == 2) && proxies[slot] == -1) {
            operation = 0;
        }
        uint32_t input[8] = {operation, (uint32_t)slot};
        input[2] = random_float(&state, 120, 13, 1);
        input[3] = random_float(&state, 120, 13, 1);
        input[4] = add_float(input[2], random_float(&state, 115, 13, 0));
        input[5] = add_float(input[3], random_float(&state, 115, 13, 0));
        input[6] = random_float(&state, 116, 13, 1);
        input[7] = random_float(&state, 116, 13, 1);
        if (operation == 1 && index % 4 == 0) {
            const uint32_t *node =
                (const uint32_t *)(uintptr_t)tree[1] + (size_t)proxies[slot] * 9;
            for (size_t field = 0; field < 4; ++field) {
                input[2 + field] = node[field];
            }
        }
        if (index == 1 || index == 2) {
            for (size_t field = 2; field < 6; ++field) {
                input[field] = index == 1 ? 0 : UINT32_C(0x80000000);
            }
        }
        print_words(input, 8);
        uint32_t result = 0;
        if (operation == 0) {
            proxies[slot] = create(tree, input + 2, fixtures[slot]);
            result = (uint32_t)proxies[slot];
        } else if (operation == 1) {
            result = move(tree, proxies[slot], input + 2, input + 6);
        } else if (operation == 2) {
            destroy(tree, proxies[slot]);
            proxies[slot] = -1;
        } else if (operation == 3) {
            shift(tree, input + 6);
        }
        print_words(&result, 1);
        print_tree_state(tree, fixtures);
        uint32_t query_data[5] = {0xc4800000, 0xc4800000, 0x44800000, 0x44800000,
                                  (uint32_t)(index % 7 ? 65 : 1 + index % 64)};
        if (index % 3) {
            for (size_t field = 0; field < 4; ++field) {
                query_data[field] = input[2 + field];
            }
        }
        uint32_t vtable[] = {0, 0, (uint32_t)(uintptr_t)tree_query_callback};
        uint32_t callback[67] = {(uint32_t)(uintptr_t)vtable, 0, query_data[4]};
        uint32_t wrapper[] = {(uint32_t)(uintptr_t)tree, (uint32_t)(uintptr_t)callback};
        query(tree, wrapper, query_data);
        print_words(query_data, 5);
        print_words(callback + 1, 1);
        if (callback[1]) {
            print_words(callback + 3, callback[1]);
        }
    }
    close_tree(tree);
    semihost(4, "AB tree reference complete\n");
}

#endif
