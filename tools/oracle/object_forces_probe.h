#ifndef AB_ORACLE_OBJECT_FORCES_H
#define AB_ORACLE_OBJECT_FORCES_H

#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
#include "object_force_audio_context.h"
#endif

#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
static uint32_t force_interval_sprite(void *lua, const char *field) {
    damage_lua.get_field(lua, -1, field);
    uint32_t value = damage_named(lua, -1, "rest")     ? 1u
                     : damage_named(lua, -1, "active") ? 2u
                     : damage_named(lua, -1, "saved")  ? 3u
                                                       : 0u;
    damage_lua.set_top(lua, 2);
    return value;
}
#endif

static void probe_object_forces(void) {
    typedef void *(*Allocate)(unsigned);
    typedef void (*Construct)(void *);
    typedef void (*TableConstruct)(void *, void *);
    typedef void (*Reference)(void *);
    typedef void (*UpdateForces)(void *, void *, float);
    Allocate allocate = (Allocate)find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    TableConstruct table_construct =
        (TableConstruct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Reference claim = (Reference)find_function("_ZN4lang6Object5claimEv");
    Reference release = (Reference)find_function("_ZN4lang6Object7releaseEv");
    Reference get_reference =
        (Reference)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    UpdateForces update =
        (UpdateForces)find_function("_ZN7GameLua16updateForceAdderER10GameObjectf");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate native object-force Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    static uint32_t game[0x1860 / 4], world[0x19240 / 4], object[50], body[38];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {5, 5, UINT32_MAX, "first"};
    object[11] = (uint32_t)(uintptr_t)body;
    object[32] = (uint32_t)(uintptr_t)name.bytes;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
    table_construct((uint8_t *)game + 0x17f8, wrapper);
#endif
    table_construct((uint8_t *)game + 0x1810, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
    get_reference((uint8_t *)game + 0x17f8);
    get_reference((uint8_t *)game + 0x1810);
    damage_lua.set_field(lua, -2, "world");
    damage_lua.set_top(lua, 0);
    static uint32_t node[6];
    node[0] = 1;
    node[1] = (uint32_t)(uintptr_t)((uint8_t *)game + 0x17a8);
    node[4] = (uint32_t)(uintptr_t)name.bytes;
    node[5] = (uint32_t)(uintptr_t)object;
    game[0x17ac / 4] = game[0x17b0 / 4] = game[0x17b4 / 4] = (uint32_t)(uintptr_t)node;
    game[0x17b8 / 4] = 1;
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
    force_audio_open(game);
#endif
    uint32_t state = UINT32_C(0x520f0ac3);
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
    semihost(4, "AB_ANDROID_OBJECT_FORCES_AUDIO_V1 4096\n");
#elif defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
    semihost(4, "AB_ANDROID_OBJECT_FORCES_INTERVAL_V1 4096\n");
#elif defined(AB_ORACLE_OBJECT_FORCES_RELATIVE)
    semihost(4, "AB_ANDROID_OBJECT_FORCES_RELATIVE_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_OBJECT_FORCES_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[36] = {(uint32_t)(item % 3), (uint32_t)((item / 3) & 1),
                              (uint32_t)((item / 6) & 1), (uint32_t)((item / 12) % 6),
                              (uint32_t)((item / 72) & 15)};
        for (size_t field = 5; field < 22; ++field) {
            input[field] = random_float(&state, 122, 8, field != 6);
        }
        if (item < 128) {
            input[4] = 15;
            for (size_t field = 7; field < 21; ++field) {
                input[field] = item & (1u << (field % 6)) ? UINT32_C(0x80000000) : 0;
            }
        } else if (item < 384) {
            static const uint32_t velocities[] = {0,
                                                  UINT32_C(0x80000000),
                                                  UINT32_C(0x3f7fffff),
                                                  UINT32_C(0x3f800000),
                                                  UINT32_C(0x3f800001),
                                                  UINT32_C(0xbf7fffff),
                                                  UINT32_C(0xbf800000),
                                                  UINT32_C(0xbf800001)};
            input[4] = 15;
            input[6] = input[9] = UINT32_C(0x3f800000);
            input[7] = input[8] = 0;
            input[10] = item & 1 ? UINT32_C(0xbf800000) : UINT32_C(0x3f800000);
            input[11] = item & 2 ? UINT32_C(0xbf800000) : UINT32_C(0x3f800000);
            input[12] = item & 4 ? UINT32_C(0xbf800000) : UINT32_C(0x3f800000);
            input[13] = velocities[(item / 8) % 8];
            input[14] = velocities[(item / 16) % 8];
        }
#if defined(AB_ORACLE_OBJECT_FORCES_RELATIVE) ||                                       \
    defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
        input[22] = random_float(&state, 118, 18, 1);
        input[23] = random_float(&state, 122, 8, 1);
        input[24] = (uint32_t)(item % 5 != 0);
        if (item < 128) {
            input[22] = item & 16 ? UINT32_C(0x80000000) : 0;
            input[23] = item & 32 ? UINT32_C(0x80000000) : 0;
        }
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
        input[25] = random_float(&state, 122, 8, 1);
        input[26] = random_float(&state, 122, 8, 1);
        input[27] = (uint32_t)((item / 9) & 3);
        input[28] = random_float(&state, 122, 8, 0);
        input[29] = random_float(&state, 122, 8, 0);
        if (item < 384) {
            static const uint32_t timers[] = {0,
                                              UINT32_C(0x80000000),
                                              UINT32_C(0x3d088889),
                                              UINT32_C(0x3f4ccccc),
                                              UINT32_C(0x3f4ccccd),
                                              UINT32_C(0x3f800000),
                                              UINT32_C(0xbdcccccd),
                                              UINT32_C(0xbf800000)};
            input[21] = 0;
            input[25] = UINT32_C(0x3f800000);
            input[26] = timers[(item / 3) % 8];
        }
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
        input[30] = (uint32_t)(item % 5);
        input[31] = 1 + random_word(&state) % 3;
        input[32] = random_word(&state);
        input[33] = random_float(&state, 124, 5, 1);
        input[34] = (uint32_t)((item / 5) & 1);
        input[35] = 1 + random_word(&state) % 3;
#endif
        damage_lua.set_top(lua, 0);
        get_reference((uint8_t *)game + 0x1810);
        damage_lua.create_table(lua, 0, 5);
        static const char *const fields[] = {"mass", "forceX", "forceY",
                                             "targetVelocity"};
        for (size_t field = 0; field < 4; ++field) {
            if (input[4] & (1u << field)) {
                damage_number(lua, -2, fields[field], input[9 + field]);
            }
        }
        if (input[3]) {
            if (input[3] <= 2) {
                damage_lua.push_boolean(lua, input[3] == 2);
            } else if (input[3] <= 4) {
                damage_lua.push_number(lua, input[3] == 3 ? 1 : 0);
            } else {
                damage_lua.push_string(lua, "true");
            }
            damage_lua.set_field(lua, -2, "disableForce");
        }
#if defined(AB_ORACLE_OBJECT_FORCES_RELATIVE) ||                                       \
    defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
        if (input[24]) {
            damage_number(lua, -2, "forceRelative", input[23]);
        }
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
        damage_number(lua, -2, "applyForceInterval", input[25]);
        if (input[27] & 1) {
            damage_number(lua, -2, "applyForceTimer", input[26]);
        }
        damage_lua.push_string(lua, "rest");
        damage_lua.set_field(lua, -2, "sprite");
        damage_lua.push_string(lua, "active");
        damage_lua.set_field(lua, -2, "spriteWhenForceApplied");
        if (input[27] & 2) {
            damage_lua.push_string(lua, "saved");
            damage_lua.set_field(lua, -2, "originalSprite");
        }
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
        force_audio_begin(lua, input);
#endif
        damage_lua.set_field(lua, -2, name.bytes);
        damage_lua.set_top(lua, 0);
        for (size_t field = 0; field < 38; ++field) {
            body[field] = 0;
        }
        body[0] = input[0];
        body[1] = 36 | (input[1] ? 2u : 0u);
        body[36] = input[5];
        body[29] = input[6];
        world[0x19238 / 4] = input[7];
        world[0x1923c / 4] = input[8];
        ((uint8_t *)object)[0x29] = (uint8_t)input[2];
        body[16] = input[13];
        body[17] = input[14];
        body[11] = input[15];
        body[12] = input[16];
        body[19] = input[17];
        body[20] = input[18];
        body[21] = input[19];
        body[18] = input[20];
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
        body[14] = input[22];
        body[30] = input[28];
        body[32] = input[29];
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
        print_words(input, 36);
#else
        print_words(input, 30);
#endif
#elif defined(AB_ORACLE_OBJECT_FORCES_RELATIVE)
        body[14] = input[22];
        print_words(input, 25);
#else
        print_words(input, 22);
#endif
        FloatBits seconds = {.bits = input[21]};
        update(game, object, seconds.number);
        uint32_t output[] = {body[1],  body[19], body[20], body[21],
                             body[36], body[16], body[17], body[18]};
        print_words(output, 8);
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
        get_reference((uint8_t *)game + 0x1810);
        damage_lua.get_field(lua, -1, name.bytes);
        damage_lua.get_field(lua, -1, "applyForceTimer");
        uint32_t timer_present = damage_lua.type(lua, -1) == 3;
        FloatBits timer = {.number = damage_lua.to_number(lua, -1)};
        damage_lua.set_top(lua, 2);
        uint32_t interval_output[] = {timer_present, timer.bits,
                                      force_interval_sprite(lua, "sprite"),
                                      force_interval_sprite(lua, "originalSprite")};
        print_words(interval_output, 4);
        damage_lua.set_top(lua, 0);
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
        force_audio_snapshot();
#endif
    }
    table_destroy((uint8_t *)game + 0x1810);
#if defined(AB_ORACLE_OBJECT_FORCES_INTERVAL)
    table_destroy((uint8_t *)game + 0x17f8);
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_AUDIO)
    force_audio_close();
#endif
    release(wrapper);
    semihost(4, "AB object forces reference complete\n");
}

#endif
