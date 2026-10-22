#ifndef AB_ORACLE_RANDOM_H
#define AB_ORACLE_RANDOM_H

typedef unsigned short *(*RandomSeed48)(const unsigned short *);

static void random_snapshot(RandomSeed48 seed, uint32_t output[2]) {
    const unsigned short zero[3] = {0};
    const unsigned short *previous = seed(zero);
    unsigned short copy[3] = {previous[0], previous[1], previous[2]};
    output[0] = (uint32_t)copy[0] | (uint32_t)copy[1] << 16;
    output[1] = copy[2];
    seed(copy);
}

static int random_contains(const char *text, const char *wanted) {
    for (size_t offset = 0; text[offset]; ++offset) {
        size_t index = 0;
        while (wanted[index] && text[offset + index] == wanted[index]) {
            ++index;
        }
        if (!wanted[index]) {
            return 1;
        }
    }
    return 0;
}

#if defined(AB_ORACLE_RANDOM_STARTUP_ONLY)
static void probe_random_startup(void) {
    typedef long (*Next)(void);
    typedef long (*Clock)(long *);
    Next next = (Next)find_function("lrand48");
    Clock clock_seconds = (Clock)find_function("time");
    RandomSeed48 seed48 = (RandomSeed48)find_function("seed48");
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate native startup-seed state\n");
    }
    damage_lua.open_math(lua);
    damage_lua.get_field(lua, -1, "randomseed");
    damage_lua.set_field(lua, -10002, "_ab_seed");
    damage_lua.set_top(lua, 0);
    int (*open_os)(void *) =
        (int (*)(void *))find_function("_ZN3lua6detail10luaopen_osEP9lua_State");
    open_os(lua);
    damage_lua.get_field(lua, -1, "time");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    if (!closure || (closure[1] & 255) != 6 || ((closure[1] >> 16) & 255) != 1) {
        fail("Expected original os.time C closure\n");
    }
    uintptr_t bias = (uintptr_t)damage_lua.new_state - UINT32_C(0x81ff0c);
    uint32_t original_time = closure[4] - (uint32_t)bias;
    damage_lua.set_field(lua, -10002, "_ab_time");
    damage_lua.set_top(lua, 0);
    uint32_t clock[4] = {original_time, (uint32_t)clock_seconds(NULL), 0, 0};
    damage_lua.get_field(lua, -10002, "_ab_time");
    if (damage_lua.call(lua, 0, 1, 0) || damage_lua.type(lua, -1) != 3) {
        fail("Original zero-argument os.time failed\n");
    }
    FloatBits actual_time = {.number = damage_lua.to_number(lua, -1)};
    clock[2] = actual_time.bits;
    clock[3] = (uint32_t)clock_seconds(NULL);
    semihost(4, "AB_ANDROID_RANDOM_STARTUP_SEED_V1 65536\n");
    print_words(clock, 4);
    uint32_t random = UINT32_C(0x52051eed);
    static const uint32_t edges[] = {
        0,          1,          0xffffffff, 0x7fffffff, 0x80000000,
        0x7fffffbf, 0x7fffffc0, 0x7fffffc1, 0x01000000, 0x01000001,
        0x01000002, 0x01000003, 0xfeffffff, 0x80000001,
    };
    for (size_t item = 0; item < 65536; ++item) {
        union {
            uint32_t bits;
            int32_t signed_word;
        } epoch = {.bits = random_word(&random)};
        if (item < sizeof(edges) / sizeof(*edges)) {
            epoch.bits = edges[item];
        }
        FloatBits time_number = {.number = (float)epoch.signed_word};
        uint32_t output[8] = {epoch.bits, time_number.bits, 0, 0, 0, 0, 0, 0};
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_seed");
        if (epoch.signed_word == -1) {
            /* Original os.time returns nil for the time() error sentinel. */
            damage_lua.push_nil(lua);
        } else {
            damage_lua.push_number(lua, time_number.number);
        }
        int status = damage_lua.call(lua, 1, 0, 0);
        if (status != (epoch.signed_word == -1 ? 2 : 0)) {
            fail("Unexpected original startup randomseed result\n");
        }
        output[2] = (uint32_t)status;
        random_snapshot(seed48, output + 3);
        output[5] = (uint32_t)next();
        random_snapshot(seed48, output + 6);
        print_words(output, 8);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB startup seed reference complete\n");
}
#endif

static void probe_random(void) {
#if defined(AB_ORACLE_RANDOM_STARTUP_ONLY)
    probe_random_startup();
    return;
#endif
    typedef void (*Seed)(long);
    typedef long (*Next)(void);
    Seed seed = (Seed)find_function("srand48");
    Next next = (Next)find_function("lrand48");
    RandomSeed48 seed48 = (RandomSeed48)find_function("seed48");
    semihost(4, "AB_ANDROID_RANDOM_V1 4096 65536\n");
    uint32_t random = UINT32_C(0x52048a1d);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t words[21];
        words[0] = random_word(&random);
        seed((long)(int32_t)words[0]);
        random_snapshot(seed48, words + 1);
        for (size_t draw = 0; draw < 16; ++draw) {
            words[3 + draw] = (uint32_t)next();
        }
        random_snapshot(seed48, words + 19);
        print_words(words, 21);
    }
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate controlled native math.random state\n");
    }
    damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "math");
    damage_lua.get_field(lua, -1, "random");
    damage_lua.set_field(lua, -10002, "_ab_random");
    damage_lua.set_top(lua, 0);
    for (size_t item = 0; item < 65536; ++item) {
        uint32_t high = random_word(&random) & 65535;
        uint32_t low = random_word(&random);
        uint64_t incoming = (uint64_t)high << 32 | low;
        if (item < 512) {
            static const uint32_t wanted[] = {0,          1,          0x7fffffff,
                                              0x7ffffffe, 0x7fffffc0, 0x7fffffbf,
                                              0x40000000, 0x3fffffff};
            uint64_t selected = (uint64_t)wanted[(item / 4) % 8] << 17;
            selected |= random_word(&random) & 0x1ffff;
            incoming =
                ((selected - 11) * UINT64_C(0xdfe05bcb1365)) & UINT64_C(0xffffffffffff);
        }
        static const int32_t bounds[][2] = {{1, 36},
                                            {36, 10000},
                                            {0, 0},
                                            {-1, 1},
                                            {10000, 1},
                                            {16777218, 16777220},
                                            {-2147483647 - 1, 2147483520},
                                            {-1073741824, 1073741824},
                                            {2147483520, 2147483520},
                                            {-32768, 32767},
                                            {1, 1},
                                            {-5, -1}};
        size_t selected = (item / 32) % (sizeof(bounds) / sizeof(*bounds));
        FloatBits first = {.number = (float)bounds[selected][0]};
        FloatBits second = {.number = (float)bounds[selected][1]};
        uint32_t input[] = {(uint32_t)incoming, (uint32_t)(incoming >> 32),
                            (uint32_t)item % 4, first.bits, second.bits};
        unsigned short initial[] = {(unsigned short)incoming,
                                    (unsigned short)(incoming >> 16),
                                    (unsigned short)(incoming >> 32)};
        seed48(initial);
        uint32_t output[5] = {(uint32_t)next(), 0, UINT32_C(0xdeadbeef), 0, 0};
        seed48(initial);
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_random");
        if (input[2] > 0) {
            damage_lua.push_number(lua, first.number);
        }
        if (input[2] > 1) {
            damage_lua.push_number(lua, second.number);
        }
        if (input[2] > 2) {
            damage_lua.push_number(lua, 0);
        }
        int status = damage_lua.call(lua, (int)input[2], 1, 0);
        if (!status) {
            FloatBits result = {.number = damage_lua.to_number(lua, -1)};
            output[2] = result.bits;
        } else {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (status != 2 || !message) {
                fail("Unexpected native math.random exception\n");
            }
            output[1] = random_contains(message, "wrong number of arguments") ? 1u
                        : random_contains(message, "interval is empty")       ? 2u
                                                                              : 3u;
            if (output[1] == 3) {
                fail("Unclassified native math.random error\n");
            }
        }
        random_snapshot(seed48, output + 3);
        print_words(input, 5);
        print_words(output, 5);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB random reference complete\n");
}

#endif
