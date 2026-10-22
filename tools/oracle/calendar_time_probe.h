#ifndef AB_ORACLE_CALENDAR_TIME_H
#define AB_ORACLE_CALENDAR_TIME_H

#include "reference_calendar.h"

static void probe_calendar_time(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct)(void *) = (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    int (*difference)(void *, void *) = (int (*)(void *, void *))find_function(
#if defined(AB_ORACLE_CALENDAR_PARTS_ONLY)
        "_ZN7GameLua17getTimeDifferenceEPN3lua8LuaStateE");
#else
        "_ZN7GameLua26getTimeDifferenceInSecondsEPN3lua8LuaStateE");
#endif
    int (*set_environment)(const char *, const char *, int) =
        (int (*)(const char *, const char *, int))find_function("setenv");
    void (*refresh_zone)(void) = (void (*)(void))find_function("tzset");
    if (set_environment("TZ", "UTC0", 1))
        fail("Cannot configure isolated native calendar zone\n");
    refresh_zone();
    uint32_t *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot allocate original calendar wrapper\n");
    construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    uint32_t game[5] = {0, 0, 0, 0, (uint32_t)(uintptr_t)wrapper};
    static const char *fields[] = {"year", "month",   "day",
                                   "hour", "minutes", "seconds"};
#if defined(AB_ORACLE_CALENDAR_PARTS_ONLY)
    semihost(4, "AB_ANDROID_CALENDAR_PARTS_V1 512 UTC\n");
#else
    semihost(4, "AB_ANDROID_CALENDAR_TIME_V1 512 UTC\n");
#endif
    for (unsigned index = 0; index < 512; ++index) {
        float values[12];
        uint32_t bits[16];
        reference_calendar(index, values);
        damage_lua.set_top(lua, 0);
        for (unsigned which = 0; which < 2; ++which) {
            damage_lua.create_table(lua, 0, 6);
            for (unsigned field = 0; field < 6; ++field) {
                FloatBits value = {.number = values[which * 6 + field]};
                bits[which * 6 + field] = value.bits;
                damage_number(lua, (int)which + 1, fields[field], value.bits);
            }
        }
        if (difference(game, wrapper) != 1 || damage_lua.get_top(lua) != 3)
            fail("Unexpected original calendar result shape\n");
#if defined(AB_ORACLE_CALENDAR_PARTS_ONLY)
        if (damage_lua.type(lua, 3) != 5)
            fail("Expected original calendar difference table\n");
        const char *const parts[] = {"days", "hours", "minutes", "seconds"};
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, 3, parts[field]);
            if (damage_lua.type(lua, 4) != 3)
                fail("Missing numeric native calendar component\n");
            FloatBits result = {.number = damage_lua.to_number(lua, 4)};
            bits[12 + field] = result.bits;
            damage_lua.set_top(lua, 3);
        }
#else
        if (damage_lua.type(lua, 3) != 3)
            fail("Unexpected original calendar result\n");
        FloatBits result = {.number = damage_lua.to_number(lua, 3)};
        bits[12] = result.bits;
#endif
        print_words(bits, 6);
        print_words(bits + 6, 6);
#if defined(AB_ORACLE_CALENDAR_PARTS_ONLY)
        print_words(bits + 12, 4);
#else
        print_words(bits + 12, 1);
#endif
    }
    damage_lua.set_top(lua, 0);
    release(wrapper);
#if defined(AB_ORACLE_CALENDAR_PARTS_ONLY)
    semihost(4, "AB calendar parts reference complete\n");
#else
    semihost(4, "AB calendar time reference complete\n");
#endif
}

#endif
