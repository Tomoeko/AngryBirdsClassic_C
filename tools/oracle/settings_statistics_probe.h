#ifndef AB_ORACLE_SETTINGS_STATISTICS_H
#define AB_ORACLE_SETTINGS_STATISTICS_H

#include "../../tests/settings_statistics_cases.h"

#include <stdbool.h>

static void (*statistics_push_value)(void *, int);

static void statistics_push(void *lua, uint32_t kind, uint32_t bits) {
    if (kind == 0)
        damage_lua.push_nil(lua);
    else if (kind == 1)
        damage_lua.push_boolean(lua, bits != 0);
    else if (kind == 3) {
        FloatBits number = {.bits = bits};
        damage_lua.push_number(lua, number.number);
    } else
        damage_lua.push_string(lua, "bad");
}

static void statistics_call(void *lua, StatisticsAction action, uint32_t output[3]) {
    static const char *const methods[] = {"getLocalAmount", "setLocalAmount",
                                          "getDeviceSum", "incrementBirdsShot",
                                          "incrementBackwardsBirdCount"};
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "settingsWrapper");
    damage_lua.get_field(lua, top + 1, methods[action.operation]);
    statistics_push_value(lua, top + 1);
    int arguments = 1;
    if (action.operation < STATISTICS_INCREMENT_BIRDS) {
        damage_lua.push_string(lua, statistics_counters[action.counter]);
        ++arguments;
        if (action.operation == STATISTICS_SET_LOCAL) {
            statistics_push(lua, action.kind, action.bits);
            ++arguments;
        }
    }
    int okay = damage_lua.call(lua, arguments, 1, 0) == 0;
    output[0] = (uint32_t)okay;
    output[1] = okay ? (uint32_t)damage_lua.type(lua, -1) : 0;
    output[2] = 0;
    if (output[1] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[2] = value.bits;
    }
    damage_lua.set_top(lua, top);
}

static void statistics_prepare(void *lua, uint32_t sample) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "settings");
    for (uint32_t counter = 0; counter < 2; ++counter) {
        uint32_t variant = counter ? sample / 8 : sample % 8;
        if (variant == 0)
            continue;
        if (variant < 4) {
            damage_lua.get_field(lua, -10002, "settings");
            statistics_push(lua, variant == 1 ? 1 : variant == 2 ? 3 : 4, 0);
            damage_lua.set_field(lua, 1, statistics_counters[counter]);
            damage_lua.set_top(lua, 0);
            continue;
        }
        uint32_t output[3];
        statistics_call(lua, (StatisticsAction){STATISTICS_SET_LOCAL, counter, 0, 0},
                        output);
        if (!output[0])
            fail("Original statistics fixture initialization failed\n");
        damage_lua.get_field(lua, -10002, "settings");
        damage_lua.get_field(lua, 1, statistics_counters[counter]);
        if (variant < 7) {
            statistics_push(lua, variant == 4 ? 0 : variant == 5 ? 1 : 3, 0);
            damage_lua.set_field(lua, 2, "device_fixture");
            damage_lua.set_top(lua, 0);
            continue;
        }
        damage_lua.get_field(lua, 2, "device_fixture");
        statistics_push(lua, 3, statistics_seed_values[sample % 4]);
        damage_lua.set_field(lua, 3, "installationId_fixture");
        statistics_push(lua, 3, 0x4cbebc20);
        damage_lua.set_field(lua, 3, "installationId_prior_a");
        statistics_push(lua, 3, 0xccbebc20);
        damage_lua.set_field(lua, 3, "installationId_prior_b");
        statistics_push(lua, 3, 0x3f800000);
        damage_lua.set_field(lua, 3, "installationId_prior_c");
        damage_lua.set_top(lua, 2);
        damage_lua.create_table(lua, 0, 0);
        statistics_push(lua, 3, 0x447a0000);
        damage_lua.set_field(lua, 3, "installationId_prior_c");
        damage_lua.set_field(lua, 2, "device_other");
        damage_lua.set_top(lua, 0);
    }
}

static void probe_settings_statistics(void) {
    initialize_damage_lua();
    statistics_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original statistics state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueDeviceId");
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueInstallationId");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "settings");
    load_damage_chunk(lua, "/data/local/tmp/ab520/statistics/settings.lua");
    semihost(4, "AB_ANDROID_SETTINGS_STATISTICS_V1 64 16\n");
    for (uint32_t sample = 0; sample < STATISTICS_CASES; ++sample) {
        statistics_prepare(lua, sample);
        print_words(&sample, 1);
        for (uint32_t step = 0; step < STATISTICS_ACTIONS; ++step) {
            uint32_t output[15] = {0};
            statistics_call(lua, statistics_actions[step], output);
            for (uint32_t counter = 0; counter < 2; ++counter) {
                statistics_call(lua,
                                (StatisticsAction){STATISTICS_GET_LOCAL, counter, 0, 0},
                                output + 3 + 6 * counter);
                statistics_call(lua,
                                (StatisticsAction){STATISTICS_GET_SUM, counter, 0, 0},
                                output + 6 + 6 * counter);
            }
            print_words(output, 15);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB settings statistics reference complete\n");
}

#endif
