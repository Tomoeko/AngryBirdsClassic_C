#ifndef AB_ORACLE_WORLD_ITERATION_H
#define AB_ORACLE_WORLD_ITERATION_H

#include "reference_world_iteration.h"

static void wi_set(void *lua, unsigned key, unsigned value) {
    if (value) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_number(lua, (float)value);
        damage_lua.set_field(lua, -2, "id");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, wi_keys[key]);
}

static uint32_t wi_value(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0)
        return 0;
    if (damage_lua.type(lua, index) != 5)
        fail("Original world next returned a non-table value\n");
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, index, "id");
    uint32_t result = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return result;
}

static void probe_world_iteration(void) {
    semihost(4, "AB_ANDROID_WORLD_ITERATION_V1 24 2\n");
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate native world iteration state\n");
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    void (*push)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    int (*collect)(void *, int, int) =
        (int (*)(void *, int, int))find_function("_Z6lua_gcP9lua_Stateii");
    /* Pin the same key identities that the C binding borrows, including across
     * explicit collection of nil-valued nodes with the current key on stack. */
    damage_lua.create_table(lua, 0, WI_KEYS);
    for (unsigned key = 0; key < WI_KEYS; ++key) {
        damage_lua.push_string(lua, wi_keys[key]);
        damage_lua.set_field(lua, 1, wi_keys[key]);
    }
    damage_lua.set_field(lua, -10002, "_ab_wi_keys");
    for (unsigned profile = 0; profile < WI_PROFILES; ++profile) {
        uint32_t records[2 * WI_MAX_STEPS][WI_RECORD_WORDS];
        unsigned count = 0;
        damage_lua.create_table(lua, 0, (int)wi_hint(profile));
        for (unsigned order = 0; order < wi_initial_count(profile); ++order) {
            unsigned key = wi_initial_key(profile, order);
            wi_set(lua, key, key + 1);
        }
        for (unsigned phase = 0; phase < 2; ++phase) {
            damage_lua.set_top(lua, 1);
            damage_lua.push_nil(lua);
            for (unsigned step = 0; step < WI_MAX_STEPS; ++step) {
                /* Keep native errors inside the original unwindable frames;
                 * a freestanding C callback cannot carry the target's throw. */
                damage_lua.get_field(lua, -10002, "next");
                push(lua, 1);
                push(lua, 2);
                int status = damage_lua.call(lua, 2, 2, 0);
                uint32_t *record = records[count++];
                for (unsigned word = 0; word < WI_RECORD_WORDS; ++word)
                    record[word] = 0;
                record[0] = phase;
                record[1] = step;
                if (status) {
                    record[2] = 2;
                    damage_lua.set_top(lua, 1);
                    break;
                }
                if (damage_lua.type(lua, 3) == 0) {
                    damage_lua.set_top(lua, 1);
                    break;
                }
                unsigned key = 0;
                while (key < WI_KEYS && !damage_named(lua, 3, wi_keys[key]))
                    ++key;
                if (key == WI_KEYS)
                    fail("Original world next returned an unknown key\n");
                record[2] = 1;
                record[3] = key + 1;
                record[4] = wi_value(lua, 4);
                if (!phase) {
                    WiAction actions[16];
                    unsigned actions_count = wi_actions(profile, step, key, actions);
                    for (unsigned action = 0; action < actions_count; ++action) {
                        WiAction current = actions[action];
                        if (current.kind == 1)
                            (void)collect(lua, 2, 0);
                        else
                            wi_set(lua, current.key, current.value);
                    }
                }
                if (wi_value(lua, 4) != record[4])
                    fail("Original captured world value changed after rebinding\n");
                damage_lua.get_field(lua, 1, wi_keys[key]);
                record[5] = wi_value(lua, 5);
                damage_lua.set_top(lua, 4);
                push(lua, 3);
                void (*replace)(void *, int) =
                    (void (*)(void *, int))find_function("_Z11lua_replaceP9lua_Statei");
                replace(lua, 2);
                damage_lua.set_top(lua, 2);
                if (step + 1 == WI_MAX_STEPS)
                    fail("Original world iteration exceeded its capture bound\n");
            }
        }
        uint32_t header[] = {profile, wi_hint(profile), count};
        print_words(header, 3);
        for (unsigned index = 0; index < count; ++index)
            print_words(records[index], WI_RECORD_WORDS);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB world iteration reference complete\n");
}

#endif
