#ifndef AB_ORACLE_OBJECT_FRICTION_H
#define AB_ORACLE_OBJECT_FRICTION_H

static void probe_object_friction(void) {
    typedef void *(*Allocate)(unsigned);
    typedef void (*Construct)(void *);
    typedef void (*TableConstruct)(void *, void *);
    typedef void (*Reference)(void *);
    typedef void (*UpdateFriction)(void *, void *, float);
    Allocate allocate = (Allocate)find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    TableConstruct table_construct =
        (TableConstruct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Reference claim = (Reference)find_function("_ZN4lang6Object5claimEv");
    Reference release = (Reference)find_function("_ZN4lang6Object7releaseEv");
    Reference get_reference =
        (Reference)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    UpdateFriction update =
        (UpdateFriction)find_function("_ZN7GameLua14updateFrictionER10GameObjectf");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate native object-friction Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    static uint32_t game[0x1860 / 4], object[50], body[38];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {5, 5, UINT32_MAX, "first"};
    object[11] = (uint32_t)(uintptr_t)body;
    object[32] = (uint32_t)(uintptr_t)name.bytes;
    table_construct((uint8_t *)game + 0x1810, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    uint32_t state = UINT32_C(0x520f12c7);
    semihost(4, "AB_ANDROID_OBJECT_FRICTION_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17] = {(uint32_t)(item % 3), (uint32_t)((item / 3) & 1)};
        for (size_t field = 2; field < 16; ++field) {
            input[field] = random_float(&state, 122, 8, 1);
        }
        input[16] = (uint32_t)((item / 6) & 7);
        if (item % 4 == 0) {
            input[3] = item & 4 ? UINT32_C(0x80000000) : 0;
        }
        if (item < 192) {
            input[3] = item & 8 ? UINT32_C(0x80000000) : 0;
            for (size_t field = 4; field < 15; ++field) {
                input[field] = item & (1u << (field % 6)) ? UINT32_C(0x80000000) : 0;
            }
            input[6] = item & 32 ? UINT32_C(0x3f800000) : input[6];
            input[7] = item & 64 ? UINT32_C(0xbf800000) : input[7];
        }
        damage_lua.set_top(lua, 0);
        get_reference((uint8_t *)game + 0x1810);
        damage_lua.create_table(lua, 0, 3);
        static const char *const fields[] = {"airResistance", "frictionXRelative",
                                             "frictionYRelative"};
        for (size_t field = 0; field < 3; ++field) {
            if (input[16] & (1u << field)) {
                damage_number(lua, -2, fields[field], input[3 + field]);
            }
        }
        damage_lua.set_field(lua, -2, name.bytes);
        damage_lua.set_top(lua, 0);
        for (size_t field = 0; field < 38; ++field) {
            body[field] = 0;
        }
        body[0] = input[0];
        body[1] = 36 | (input[1] ? 2u : 0u);
        body[36] = input[2];
        body[5] = input[6];
        body[6] = input[7];
        body[16] = input[8];
        body[17] = input[9];
        body[11] = input[10];
        body[12] = input[11];
        body[19] = input[12];
        body[20] = input[13];
        body[21] = input[14];
        print_words(input, 17);
        FloatBits seconds = {.bits = input[15]};
        update(game, object, seconds.number);
        uint32_t output[] = {body[1], body[19], body[20], body[21], body[36]};
        print_words(output, 5);
    }
    table_destroy((uint8_t *)game + 0x1810);
    release(wrapper);
    semihost(4, "AB object friction reference complete\n");
}

#endif
