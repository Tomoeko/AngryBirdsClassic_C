#ifndef AB_ORACLE_BLOCK_UPDATE_H
#define AB_ORACLE_BLOCK_UPDATE_H

#include "selected_function.h"

static uint32_t bu_input[12], bu_events[16][6], bu_event_count, bu_handlers,
    bu_registered, bu_selected, bu_appended;
static const char *const bu_scripts[] = {"_ab_bu_script1", "_ab_bu_script2"};
static void (*bu_raw_set)(void *, int, int);

static int bu_handler(void *lua);

static void bu_methods(void *lua) {
    int top = damage_lua.get_top(lua);
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, bu_scripts[index]);
        if (bu_handlers & (1u << index)) {
            damage_lua.push_closure(lua, bu_handler, 0);
        } else {
            damage_lua.push_boolean(lua, 0);
        }
        damage_lua.set_field(lua, top + 1, "update");
        damage_lua.set_top(lua, top);
    }
}

static void bu_event_name(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "events");
    damage_lua.push_string(lua, bu_selected ? "skip" : "evt");
    damage_lua.set_field(lua, top + 2, "EID_UPDATE_BLOCK");
    damage_lua.set_top(lua, top);
}

static void bu_record(void *lua, uint32_t id, int first, uint32_t selected) {
    if (damage_lua.get_top(lua) != 3 || bu_event_count >= 16 ||
        damage_lua.type(lua, first) != 3 || damage_lua.type(lua, first + 1) != 3) {
        fail("Unexpected original block-update callback arguments\n");
    }
    FloatBits seconds = {.number = damage_lua.to_number(lua, first)},
              time = {.number = damage_lua.to_number(lua, first + 1)};
    uint32_t *record = bu_events[bu_event_count++];
    record[0] = id;
    record[1] = seconds.bits;
    record[2] = time.bits;
    record[3] = bu_handlers;
    record[4] = bu_registered;
    record[5] = selected;
}

static int bu_handler(void *lua) {
    damage_lua.get_field(lua, 1, "id");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, 3);
    if (id < 1 || id > 2) {
        fail("Unexpected original block-update script identity\n");
    }
    bu_record(lua, id, 2, bu_selected);
    if (bu_input[3] & 1) {
        bu_handlers ^= 1u << (2 - id);
    }
    if (bu_input[3] & 2) {
        bu_selected ^= 1;
        bu_event_name(lua);
    }
    if ((bu_input[3] & 4) && !bu_appended) {
        bu_appended = 1;
        damage_lua.get_field(lua, -10002, "_ab_bu_list");
        damage_lua.get_field(lua, -10002, bu_scripts[1]);
        bu_raw_set(lua, 4, (int)++bu_registered);
        damage_lua.set_top(lua, 3);
    }
    if (bu_input[3] & 8) {
        bu_handlers &= ~(1u << (id - 1));
    }
    bu_methods(lua);
    return 0;
}

static int bu_global(void *lua) {
    uint32_t selected;
    if (damage_named(lua, 1, "evt")) {
        selected = 0;
    } else if (damage_named(lua, 1, "skip")) {
        selected = 1;
    } else {
        fail("Unexpected original block-update global event\n");
    }
    bu_record(lua, 0, 2, selected);
    return 0;
}

static void probe_block_update(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original block-update state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/manager-update.lua",
                           "_ab_bu_update");
    bu_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    const char *(*upvalue)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "triggerGlobalEvent", bu_global);
    damage_lua.set_field(lua, -10002, "BlockScriptManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, 1, "events");
    damage_lua.set_field(lua, -10002, "gamelua");
    uint32_t random = UINT32_C(0x520b10c5);
    semihost(4, "AB_ANDROID_BLOCK_UPDATE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        bu_input[0] = random_float(&random, 112, 30, 1);
        bu_input[1] = random_float(&random, 112, 30, 1);
        if (item % 31 == 0) {
            bu_input[1] = UINT32_C(0x7fc12345);
        }
        bu_input[2] = (uint32_t)(item % 9);
        bu_input[3] = (uint32_t)((item / 9) % 16);
        bu_input[4] = (uint32_t)((item / 144) % 7);
        bu_input[5] = (uint32_t)((item / 1008) & 1);
        for (size_t index = 0; index < 6; ++index) {
            bu_input[6 + index] = random_word(&random) % 2 + 1;
        }
        bu_registered = bu_input[4];
        bu_selected = bu_input[5];
        bu_handlers = bu_event_count = bu_appended = 0;
        for (size_t index = 0; index < 2; ++index) {
            uint32_t kind = index ? bu_input[2] / 3 : bu_input[2] % 3;
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_number(lua, (float)index + 1);
            damage_lua.set_field(lua, 1, "id");
            if (kind == 2) {
                bu_handlers |= 1u << index;
                damage_lua.push_closure(lua, bu_handler, 0);
                damage_lua.set_field(lua, 1, "update");
            } else if (kind == 1) {
                damage_lua.push_boolean(lua, 0);
                damage_lua.set_field(lua, 1, "update");
            }
            damage_lua.set_field(lua, -10002, bu_scripts[index]);
        }
        bu_event_name(lua);
        damage_lua.create_table(lua, 0, 0);
        for (uint32_t index = 0; index < bu_registered; ++index) {
            damage_lua.get_field(lua, -10002, bu_scripts[bu_input[6 + index] - 1]);
            bu_raw_set(lua, 1, (int)index + 1);
        }
        damage_lua.set_field(lua, -10002, "_ab_bu_list");
        damage_lua.get_field(lua, -10002, "_ab_bu_update");
        damage_lua.get_field(lua, -10002, "_ab_bu_list");
        if (!upvalue(lua, 1, 1)) {
            fail("Cannot bind original block-update registrations\n");
        }
        FloatBits seconds = {.bits = bu_input[0]}, time = {.bits = bu_input[1]};
        damage_lua.push_number(lua, seconds.number);
        damage_lua.push_number(lua, time.number);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original block-update phase failed\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t output[] = {bu_event_count, bu_handlers, bu_registered, bu_selected,
                             bu_appended};
        print_words(bu_input, 12);
        print_words(output, 5);
        for (size_t index = 0; index < bu_event_count; ++index) {
            print_words(bu_events[index], 6);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block update reference complete\n");
}

#endif
