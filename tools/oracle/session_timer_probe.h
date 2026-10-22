#ifndef AB_ORACLE_SESSION_TIMER_H
#define AB_ORACLE_SESSION_TIMER_H

#include "session_calendar_probe.h"
#include "reference_session_timer.h"

static float st_duration[3];

static void st_method(void *lua, const char *method) {
    damage_lua.get_field(lua, -10002, "_ab_timer");
    damage_lua.get_field(lua, -1, method);
    damage_lua.get_field(lua, -10002, "_ab_timer");
}

static void st_start(void *lua, unsigned profile) {
    st_method(lua, "startTimer");
    damage_lua.get_field(lua, -10002, "_ab_duration");
    if (profile & 1)
        damage_lua.get_field(lua, -10002, "_ab_start");
    else
        damage_lua.push_boolean(lua, 0);
    st_call(lua, "startTimer", 3, 0);
    damage_lua.set_top(lua, 0);
}

static void st_action(void *lua, unsigned profile, unsigned step) {
    if (step == 1 || step == 11 || step == 15)
        st_start(lua, profile);
    else if (step == 3) {
        damage_lua.get_field(lua, -10002, "_ab_duration");
        FloatBits value = {.number = st_duration[2] + 1};
        damage_number(lua, 1, "seconds", value.bits);
        damage_lua.set_top(lua, 0);
        if (profile & 1) {
            damage_lua.get_field(lua, -10002, "_ab_start");
            value.number = st_date[5] + 1;
            damage_number(lua, 1, "seconds", value.bits);
        }
    } else if (step == 4) {
        st_method(lua, "saveToTable");
        st_call(lua, "saveToTable", 1, 1);
        damage_lua.set_field(lua, -10002, "_ab_saved");
    } else if (step == 5 || step == 8 || step == 12 || step == 16) {
        st_method(lua, "loadFromTable");
        if (step == 8)
            damage_lua.get_field(lua, -10002, "_ab_saved");
        else if (step == 12) {
            damage_lua.create_table(lua, 0, 1);
            damage_lua.get_field(lua, -10002, "_ab_start");
            damage_lua.set_field(lua, -2, "startTime");
        } else if (step == 5)
            damage_lua.push_nil(lua);
        else
            damage_lua.push_boolean(lua, 0);
        st_call(lua, "loadFromTable", 2, 0);
    } else if (step == 6 || step == 14 || step == 17) {
        st_method(lua, "setActive");
        if (step == 6)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_boolean(lua, step == 17);
        st_call(lua, "setActive", 2, 0);
    } else if (step == 7 || step == 10) {
        const char *method = step == 7 ? "init" : "forceComplete";
        st_method(lua, method);
        st_call(lua, method, 1, 0);
    } else if (step == 9) {
        damage_lua.get_field(lua, -10002, "_ab_saved");
        damage_lua.get_field(lua, 1, "startTime");
        damage_number(lua, 2, "seconds", UINT32_C(0x42ce0000));
        damage_lua.get_field(lua, 1, "duration");
        damage_number(lua, 3, "seconds", UINT32_C(0xc2ca0000));
    }
    damage_lua.set_top(lua, 0);
}

static void st_snapshot(void *lua, unsigned profile, unsigned step) {
    uint32_t state[24] = {0}, results[11] = {0}, save[11] = {0};
    unsigned clock_before = st_clock_calls;
    damage_lua.get_field(lua, -10002, "_ab_timer");
    st_field(lua, 1, "active", state);
    st_field(lua, 1, "forceCompleted", state + 2);
    st_field(lua, 1, "durationInSeconds", state + 4);
    st_table_words(lua, 1, "startTime", st_date_fields, 6, state + 6);
    st_table_words(lua, 1, "duration", st_duration_fields, 3, state + 13);
    damage_lua.set_top(lua, 0);
    if (state[6] && state[13]) {
        st_method(lua, "getEndTime");
        st_call(lua, "getEndTime", 1, 1);
        state[17] = 1;
        for (unsigned index = 0; index < 6; ++index) {
            uint32_t value[2];
            st_field(lua, 2, st_date_fields[index], value);
            if (value[0] != 3)
                fail("Nonnumeric original session end date\n");
            state[18 + index] = value[1];
        }
        damage_lua.set_top(lua, 0);
    }
    const char *methods[] = {"getTimeUntilDoneInSeconds", "isComplete", "getProgress",
                             "getTimeUntilDone"};
    for (unsigned index = 0; index < 4; ++index) {
        st_method(lua, methods[index]);
        st_call(lua, methods[index], 1, 1);
        if (index < 2)
            st_value(lua, 2, results + index * 2);
        else if (index == 2) {
            if (damage_lua.type(lua, 2) != 3)
                fail("Nonnumeric original session progress\n");
            FloatBits value = {.number = damage_lua.to_number(lua, 2)};
            /* Compare IEEE NaN classification; payload/sign are host details. */
            results[4] = (value.bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)
                             ? UINT32_C(0x7fc00000)
                             : value.bits;
        } else {
            results[5] = (uint32_t)damage_lua.type(lua, 2);
            if (results[5] == 4) {
                unsigned length;
                const char *text = damage_lua.to_string(lua, 2, &length);
                if (!text || length > 16)
                    fail("Original session formatted string exceeds bound\n");
                results[6] = length;
                for (unsigned byte = 0; byte < length; ++byte)
                    results[7 + byte / 4] |= (uint32_t)(uint8_t)text[byte]
                                             << ((byte % 4) * 8);
            } else if (results[5] != 3 || damage_lua.to_number(lua, 2) != 0)
                fail("Unexpected original inactive formatted result\n");
        }
        damage_lua.set_top(lua, 0);
    }
    st_method(lua, "saveToTable");
    st_call(lua, "saveToTable", 1, 1);
    if (damage_lua.type(lua, 2) != 5)
        fail("Original session save is not a table\n");
    st_table_words(lua, 2, "startTime", st_date_fields, 6, save);
    st_table_words(lua, 2, "duration", st_duration_fields, 3, save + 7);
    damage_lua.set_top(lua, 0);
    uint32_t header[] = {profile, step, clock_before, st_clock_calls};
    print_words(header, 4);
    for (unsigned row = 0; row < 4; ++row)
        print_words(state + row * 6, 6);
    print_words(results, 6);
    print_words(results + 6, 5);
    print_words(save, 6);
    print_words(save + 6, 5);
}

static void probe_session_timer(void) {
    void *lua = st_open();
    semihost(4, "AB_ANDROID_SESSION_TIMER_V1 24 18 UTC\n");
    for (unsigned profile = 0; profile < 24; ++profile) {
        reference_session_timer(profile, st_date, st_duration);
        st_clock_calls = 0;
        st_elapsed = 0;
        st_numeric_table(lua, st_date_fields, st_date, 6);
        damage_lua.set_field(lua, -10002, "_ab_start");
        st_numeric_table(lua, st_duration_fields, st_duration, 3);
        damage_lua.set_field(lua, -10002, "_ab_duration");
        damage_lua.get_field(lua, -10002, "SessioningTimer");
        damage_lua.get_field(lua, 1, "new");
        damage_lua.get_field(lua, -10002, "SessioningTimer");
        st_call(lua, "new", 1, 1);
        if (profile % 4 == 1)
            damage_lua.push_boolean(lua, 0);
        else if (profile % 4 == 2)
            damage_lua.push_number(lua, 0);
        else if (profile % 4 == 3)
            damage_lua.push_boolean(lua, 1);
        else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 2, "forceCompleted");
        damage_lua.get_field(lua, -10002, "_ab_start");
        damage_lua.set_field(lua, 2, "startTime");
        damage_lua.get_field(lua, -10002, "_ab_duration");
        damage_lua.set_field(lua, 2, "duration");
        damage_number(lua, 2, "durationInSeconds", UINT32_C(0x42f60000));
        damage_lua.set_field(lua, -10002, "_ab_timer");
        damage_lua.set_top(lua, 0);
        for (unsigned step = 0; step < 18; ++step) {
            st_elapsed = reference_session_elapsed(step);
            st_action(lua, profile, step);
            st_snapshot(lua, profile, step);
        }
    }
    st_close();
    semihost(4, "AB session timer reference complete\n");
}

#endif
