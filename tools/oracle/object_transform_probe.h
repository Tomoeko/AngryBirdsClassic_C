#ifndef AB_ORACLE_OBJECT_TRANSFORM_H
#define AB_ORACLE_OBJECT_TRANSFORM_H

static void probe_object_transform(void) {
    typedef void (*Construct)(void *);
    typedef void (*TableConstruct)(void *, void *);
    typedef void (*WorldConstruct)(void *, const void *);
    typedef void *(*CreateBody)(void *, const void *);
    typedef void (*DestroyBody)(void *, void *);
    typedef void (*PositionSet)(void *, const char **, float, float);
    typedef void (*RotationSet)(void *, const char **, float);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    TableConstruct table_construct =
        (TableConstruct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Construct claim = (Construct)find_function("_ZN4lang6Object5claimEv");
    Construct release = (Construct)find_function("_ZN4lang6Object7releaseEv");
    Construct reference = (Construct)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    WorldConstruct world_construct =
        (WorldConstruct)find_function("_ZN7b2WorldC1ERK6b2Vec2");
    Construct world_destroy = (Construct)find_function("_ZN7b2WorldD1Ev");
    CreateBody create_body =
        (CreateBody)find_function("_ZN7b2World10CreateBodyEPK9b2BodyDef");
    DestroyBody destroy_body =
        (DestroyBody)find_function("_ZN7b2World11DestroyBodyEP6b2Body");
    PositionSet set_position =
        (PositionSet)find_function("_ZN7GameLua11setPositionESsff");
    RotationSet set_rotation =
        (RotationSet)find_function("_ZN7GameLua11setRotationESsf");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original transform Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    static uint32_t game[0x1860 / 4], object[50], node[6];
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {5, 5, UINT32_MAX, "first"};
    const char *identifier = name.bytes;
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    table_construct((uint8_t *)game + 0x1810, wrapper);
    reference((uint8_t *)game + 0x17f8);
    reference((uint8_t *)game + 0x1810);
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_top(lua, 0);
    node[0] = 1;
    node[1] = (uint32_t)(uintptr_t)((uint8_t *)game + 0x17a8);
    node[4] = (uint32_t)(uintptr_t)name.bytes;
    node[5] = (uint32_t)(uintptr_t)object;
    game[0x17ac / 4] = game[0x17b0 / 4] = game[0x17b4 / 4] = (uint32_t)(uintptr_t)node;
    game[0x17b8 / 4] = 1;
    const uint32_t gravity[2] = {0, 0};
    world_construct(world_storage, gravity);
    const size_t cache_offsets[] = {0x44 / 4, 0x50 / 4, 0x90 / 4};
    const char *const fields[] = {"x", "y", "angle"};
    const uint32_t angles[] = {0,          0x80000000, 1,          0x80000001,
                               0x40c90fdb, 0xc0c90fdb, 0x7f7fffff, 0xff7fffff};
    uint32_t random = UINT32_C(0x52070a4f);
    semihost(4, "AB_ANDROID_OBJECT_TRANSFORM_V1 4096\n");
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[24] = {(uint32_t)(sample & 1), (uint32_t)((sample / 2) % 4 != 0),
                              (uint32_t)(sample % 3), (uint32_t)((sample / 7) & 1)};
        for (size_t field = 4; field < 24; ++field) {
            input[field] = random_float(&random, 118, 16, 1);
        }
        if (sample % 8 == 0 || sample < 64) {
            input[23] = angles[(sample / 8) % 8];
            input[21] = sample & 8 ? UINT32_C(0x80000000) : 0;
            input[22] = sample & 16 ? UINT32_C(0x80000001) : 1;
        }
        if (!input[1]) {
            input[21] = UINT32_C(0x7fc12345);
            input[22] = UINT32_C(0xff812345);
            input[23] = UINT32_C(0x7fc54321);
        }
        uint32_t definition[13] = {input[2], input[4], input[5], input[6]};
        definition[9] = 1 | (input[3] << 8);
        definition[10] = 1;
        definition[12] = UINT32_C(0x3f800000);
        uint32_t *body = create_body(world_storage, definition);
        if (!body) {
            fail("Cannot create original transform body\n");
        }
        body[7] = input[7];
        body[8] = input[8];
        object[11] = input[1] ? (uint32_t)(uintptr_t)body : 0;
        for (size_t pose = 0; pose < 3; ++pose) {
            for (size_t field = 0; field < 3; ++field) {
                object[cache_offsets[pose] + field] = input[9 + pose * 3 + field];
            }
        }
        reference((uint8_t *)game + 0x1810);
        damage_lua.create_table(lua, 0, 3);
        for (size_t field = 0; field < 3; ++field) {
            damage_number(lua, 2, fields[field], input[18 + field]);
        }
        damage_lua.set_field(lua, 1, "first");
        damage_lua.set_top(lua, 0);
        print_words(input, 24);
        FloatBits x = {.bits = input[21]}, y = {.bits = input[22]},
                  angle = {.bits = input[23]};
        if (input[0]) {
            set_rotation(game, &identifier, angle.number);
        } else {
            set_position(game, &identifier, x.number, y.number);
        }
        uint32_t output[24];
        for (size_t field = 0; field < 4; ++field) {
            output[field] = body[3 + field];
        }
        for (size_t field = 0; field < 6; ++field) {
            output[4 + field] = body[9 + field];
        }
        output[10] = body[0];
        output[11] = (body[1] & 2) != 0;
        for (size_t pose = 0; pose < 3; ++pose) {
            for (size_t field = 0; field < 3; ++field) {
                output[12 + pose * 3 + field] = object[cache_offsets[pose] + field];
            }
        }
        reference((uint8_t *)game + 0x1810);
        damage_lua.get_field(lua, 1, "first");
        for (size_t field = 0; field < 3; ++field) {
            damage_lua.get_field(lua, 2, fields[field]);
            FloatBits value = {.number = damage_lua.to_number(lua, 3)};
            output[21 + field] = value.bits;
            damage_lua.set_top(lua, 2);
        }
        damage_lua.set_top(lua, 0);
        print_words(output, 24);
        destroy_body(world_storage, body);
    }
    world_destroy(world_storage);
    table_destroy((uint8_t *)game + 0x1810);
    table_destroy((uint8_t *)game + 0x17f8);
    release(wrapper);
    semihost(4, "AB object transform reference complete\n");
}

#endif
