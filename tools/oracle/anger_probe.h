#ifndef AB_ORACLE_ANGER_H
#define AB_ORACLE_ANGER_H

#include "session_calendar_probe.h"
#include "reference_anger.h"

static unsigned ag_profile, ag_step, ag_count;
static uint32_t ag_rows[64][12];
static const char *ag_state_fields[] = {
    "initialized",        "maxAngerLevel", "boostCost",           "angerLevel",
    "angerRefillCharges", "queuedRefills", "angerUsedDuringLevel"};
static const char *ag_saved_fields[] = {"maxAngerLevel", "angerLevel",
                                        "refillChargesLeft", "queuedRefills"};

static void ag_record(void *lua, unsigned kind) {
    if (ag_count == 64)
        fail("Original AngerLevel trace exceeds bound\n");
    uint32_t *row = ag_rows[ag_count++];
    for (unsigned index = 0; index < 12; ++index)
        row[index] = 0;
    row[0] = kind;
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "AngerLevel");
    unsigned table = (unsigned)top + 1;
    const char *names[] = {"angerLevel", "queuedRefills", "angerRefillCharges"};
    for (unsigned index = 0; index < 3; ++index) {
        uint32_t value[2];
        st_field(lua, (int)table, names[index], value);
        if (value[0] != 3)
            fail("Unexpected AngerLevel trace scalar\n");
        row[1 + index] = value[1];
    }
    const char *timers[] = {"angerLevelTimer", "angerRefillCooldownTimer"};
    for (unsigned index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, (int)table, timers[index]);
        st_field(lua, (int)table + 1, "active", row + 4 + index * 2);
        damage_lua.set_top(lua, (int)table);
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -1, "settings");
    damage_lua.get_field(lua, -1, "anger");
    if (damage_lua.type(lua, -1) == 5) {
        st_field(lua, (int)table + 3, "angerLevel", row + 8);
        st_field(lua, (int)table + 3, "queuedRefills", row + 10);
    }
    damage_lua.set_top(lua, top);
}

static int ag_in_level(void *lua) {
    if (damage_lua.get_top(lua))
        fail("Unexpected LevelFlow arguments\n");
    ag_record(lua, 1);
    if ((ag_profile & 4) && ag_step < 8)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_boolean(lua, (ag_profile & 1) && ag_step < 8);
    return 1;
}

static int ag_save(void *lua) {
    if (damage_lua.get_top(lua) != 3 || !damage_named(lua, 1, "settings.lua") ||
        !damage_named(lua, 2, "settings") || !damage_lua.to_boolean(lua, 3))
        fail("Unexpected original settings persistence boundary\n");
    ag_record(lua, 2);
    return 0;
}

static int ag_popup(void *lua) {
    if (damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "gamefeatures/angerlevel/AngerRefillPopup.lua"))
        fail("Unexpected original refill popup boundary\n");
    ag_record(lua, 3);
    return 0;
}

static int ag_increased(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected AngerLevel event listener arguments\n");
    damage_lua.get_field(lua, 2, "id");
    if (!damage_named(lua, 3, "increased"))
        fail("Unexpected AngerLevel event identifier\n");
    ag_record(lua, 4);
    return 0;
}

static void ag_call(void *lua, const char *name, int argument, float value) {
    damage_lua.get_field(lua, -10002, "AngerLevel");
    damage_lua.get_field(lua, -1, name);
    if (argument)
        damage_lua.push_number(lua, value);
    st_call(lua, name, argument, 0);
    damage_lua.set_top(lua, 0);
}

static void ag_settings(void *lua, unsigned reload) {
    int top = damage_lua.get_top(lua);
    float values[4], date[6];
    unsigned kinds[4], timers[2];
    reference_anger(ag_profile, reload, values, kinds, date, timers);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -1, "settings");
    if (!reload && !ag_profile)
        damage_lua.push_nil(lua);
    else {
        damage_lua.create_table(lua, 0, 6);
        int table = damage_lua.get_top(lua);
        for (unsigned index = 0; index < 4; ++index) {
            if (kinds[index] == 1)
                damage_lua.push_boolean(lua, 0);
            else
                damage_lua.push_number(lua, values[index]);
            damage_lua.set_field(lua, table, ag_saved_fields[index]);
        }
        const char *names[] = {"timer", "refillCooldownTimer"};
        for (unsigned index = 0; index < 2; ++index) {
            damage_lua.create_table(lua, 0, 2);
            if (timers[index]) {
                st_numeric_table(lua, st_date_fields, date, 6);
                damage_lua.set_field(lua, -2, "startTime");
                float duration[] = {0, index ? 30 : 5, 0};
                st_numeric_table(lua, st_duration_fields, duration, 3);
                damage_lua.set_field(lua, -2, "duration");
            }
            damage_lua.set_field(lua, table, names[index]);
        }
    }
    damage_lua.set_field(lua, top + 2, "anger");
    damage_lua.set_top(lua, top);
}

static void ag_notify(void *lua) {
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 1, "eventManager");
    damage_lua.get_field(lua, 2, "notify");
    damage_lua.get_field(lua, 1, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "late");
    damage_lua.set_field(lua, 5, "id");
    st_call(lua, "notify", 2, 0);
    damage_lua.set_top(lua, 0);
}

static void ag_snapshot(void *lua) {
    uint32_t state[48] = {0}, saved[31] = {0};
    damage_lua.get_field(lua, -10002, "AngerLevel");
    for (unsigned index = 0; index < 7; ++index)
        st_field(lua, 1, ag_state_fields[index], state + index * 2);
    const char *timers[] = {"angerLevelTimer", "angerRefillCooldownTimer"};
    for (unsigned index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, 1, timers[index]);
        uint32_t *row = state + 14 + index * 17;
        st_field(lua, 2, "active", row);
        st_field(lua, 2, "forceCompleted", row + 2);
        st_field(lua, 2, "durationInSeconds", row + 4);
        st_table_words(lua, 2, "startTime", st_date_fields, 6, row + 6);
        st_table_words(lua, 2, "duration", st_duration_fields, 3, row + 13);
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 1, "settings");
    damage_lua.get_field(lua, 2, "anger");
    saved[0] = damage_lua.type(lua, 3) == 5;
    if (saved[0]) {
        for (unsigned index = 0; index < 4; ++index)
            st_field(lua, 3, ag_saved_fields[index], saved + 1 + index * 2);
        const char *names[] = {"timer", "refillCooldownTimer"};
        for (unsigned index = 0; index < 2; ++index) {
            damage_lua.get_field(lua, 3, names[index]);
            st_table_words(lua, 4, "startTime", st_date_fields, 6,
                           saved + 9 + index * 11);
            st_table_words(lua, 4, "duration", st_duration_fields, 3,
                           saved + 16 + index * 11);
            damage_lua.set_top(lua, 3);
        }
    }
    damage_lua.set_top(lua, 0);
    uint32_t header[] = {ag_profile, ag_step, st_clock_calls, ag_count};
    print_words(header, 4);
    for (unsigned row = 0; row < 8; ++row)
        print_words(state + row * 6, 6);
    for (unsigned row = 0; row < 5; ++row)
        print_words(saved + row * 6, 6);
    print_words(saved + 30, 1);
    for (unsigned row = 0; row < ag_count; ++row) {
        print_words(ag_rows[row], 6);
        print_words(ag_rows[row] + 6, 6);
    }
}

static void probe_anger(void) {
    void *lua = st_open();
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "print", damage_require);
    load_damage_chunk(lua, "/data/local/tmp/ab520/anger-event-manager.lua");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "printError", damage_require);
    damage_function(lua, 1, "saveLuaFileWrapper", ag_save);
    damage_lua.get_field(lua, -10002, "SessioningTimer");
    damage_lua.set_field(lua, 1, "SessioningTimer");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "late");
    damage_lua.set_field(lua, 2, "EID_LOAD_LEVEL_LATE");
    damage_lua.push_string(lua, "increased");
    damage_lua.set_field(lua, 2, "EID_ANGER_LEVEL_INCREASED");
    damage_lua.set_field(lua, 1, "events");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "isInLevel", ag_in_level);
    damage_lua.set_field(lua, -10002, "LevelFlow");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "loadLuaScript", ag_popup);
    damage_lua.set_field(lua, -10002, "native");
    semihost(4, "AB_ANDROID_ANGER_V1 8 14 UTC\n");
    for (ag_profile = 0; ag_profile < 8; ++ag_profile) {
        st_clock_calls = 0;
        st_elapsed = 0;
        const float date[] = {2026, 12, 31, 23, 59, 0};
        for (unsigned index = 0; index < 6; ++index)
            st_date[index] = date[index];
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.set_field(lua, 1, "settings");
        damage_lua.get_field(lua, -10002, "EventManager");
        damage_lua.get_field(lua, 2, "new");
        damage_lua.get_field(lua, -10002, "EventManager");
        st_call(lua, "new", 1, 1);
        damage_lua.set_field(lua, 1, "eventManager");
        damage_lua.set_top(lua, 0);
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_string(lua, "observer");
        damage_lua.set_field(lua, 1, "name");
        damage_function(lua, 1, "eventTriggered", ag_increased);
        damage_lua.set_field(lua, -10002, "_ab_anger_observer");
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 1, "eventManager");
        damage_lua.get_field(lua, 2, "addEventListener");
        damage_lua.get_field(lua, 1, "eventManager");
        damage_lua.push_string(lua, "increased");
        damage_lua.get_field(lua, -10002, "_ab_anger_observer");
        st_call(lua, "addEventListener", 3, 0);
        damage_lua.set_top(lua, 0);
        load_damage_chunk(lua, "/data/local/tmp/ab520/anger-level.lua");
        ag_settings(lua, 0);
        for (ag_step = 0; ag_step < 14; ++ag_step) {
            st_elapsed = reference_anger_elapsed(ag_step);
            ag_count = 0;
            if (ag_step == 1 || ag_step == 2)
                ag_call(lua, "activateRefillCooldown", 0, 0);
            else if (ag_step == 4) {
                ag_settings(lua, 1);
                ag_call(lua, "loadFromSettings", 0, 0);
            } else if (ag_step == 7 || ag_step == 8)
                ag_notify(lua);
            else if (ag_step == 9)
                ag_call(lua, "addAngerLevel", 1, -1);
            else if (ag_step == 10)
                ag_call(lua, "saveToSettings", 0, 0);
            else if (ag_step == 11)
                ag_call(lua, "loadFromSettings", 0, 0);
            else if (ag_step == 13) {
                damage_lua.get_field(lua, -10002, "gamelua");
                damage_lua.get_field(lua, 1, "settings");
                damage_lua.push_nil(lua);
                damage_lua.set_field(lua, 2, "anger");
                damage_lua.set_top(lua, 0);
                ag_call(lua, "loadFromSettings", 0, 0);
            } else
                ag_call(lua, "update", 0, 0);
            ag_snapshot(lua);
        }
    }
    st_close();
    semihost(4, "AB AngerLevel reference complete\n");
}

#endif
