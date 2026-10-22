#ifndef AB_ORACLE_GAME_BEHAVIOR_ADD_H
#define AB_ORACLE_GAME_BEHAVIOR_ADD_H

#include "../../tests/game_behavior_add_cases.h"

static uint32_t add_input[16], add_output[52];
static const void *add_group_addresses[6];
static const char *const add_group_globals[] = {"_ab_add_group1", "_ab_add_group2",
                                                "_ab_add_group3", "_ab_add_group4",
                                                "_ab_add_group5", "_ab_add_group6"};
static void (*add_push_value)(void *, int);
static void (*add_set_index)(void *, int, int);
static void (*add_get_index)(void *, int, int);
static unsigned (*add_length)(void *, int);
static int (*add_next)(void *, int);

static void add_load_method(void *lua, const char *path, const char *global) {
    char bytes[1024];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, bytes, sizeof(bytes));
    if (size <= 0 || size == sizeof(bytes) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, bytes, (unsigned)size, "original-behavior-construction"))
        fail("Cannot load original behavior construction method\n");
    damage_lua.set_field(lua, -10002, global);
}

static uint32_t add_id(void *lua, int index) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, index, "_id");
    if (damage_lua.type(lua, -1) != 3)
        fail("Missing behavior construction identity\n");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return id;
}

static void add_instance(void *lua, uint32_t id) {
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, (float)id);
    damage_lua.set_field(lua, -2, "_id");
}

static uint32_t add_group_id(void *lua, int index) {
    if (damage_lua.type(lua, index) != 5)
        return 0;
    const void *pointer = damage_lua.to_pointer(lua, index);
    for (uint32_t group = 0; group < 6; ++group)
        if (pointer == add_group_addresses[group])
            return group + 1u;
    if (add_group_addresses[4])
        fail("Unexpected additional behavior construction group\n");
    add_group_addresses[4] = pointer;
    add_push_value(lua, index);
    damage_lua.set_field(lua, -10002, add_group_globals[4]);
    return 5;
}

static int add_factory(void *lua) {
    if (damage_lua.get_top(lua) != 4 ||
        !damage_named(lua, 2, behavior_add_names[add_input[5]]) ||
        add_id(lua, 3) != 101)
        fail("Unexpected original behavior factory arguments\n");
    ++add_output[1];
    add_output[2] = add_input[5] + 1u;
    add_output[3] = add_id(lua, 3);
    uint32_t tag = (uint32_t)damage_lua.type(lua, 4);
    add_output[4] = tag;
    if (tag == 1)
        add_output[5] = damage_lua.to_boolean(lua, 4) != 0;
    else if (tag == 3) {
        FloatBits number = {.number = damage_lua.to_number(lua, 4)};
        add_output[5] = number.bits;
    } else if (tag == 4) {
        if (!damage_named(lua, 4, "payload"))
            fail("Unexpected original behavior factory text\n");
        add_output[5] = 1;
    } else if (tag == 5)
        add_output[5] = add_id(lua, 4);
    else if (tag != 0)
        fail("Unexpected original behavior factory property type\n");
    damage_lua.get_field(lua, 3, "behaviors");
    damage_lua.get_field(lua, 5, behavior_add_names[add_input[5]]);
    add_output[48] = add_group_id(lua, 6);
    if (!add_output[48])
        fail("Factory reached without a published original group\n");
    add_push_value(lua, 6);
    damage_lua.set_field(lua, -10002, "_ab_add_captured");
    if (add_input[10] & 1u) {
        damage_lua.get_field(lua, -10002, add_group_globals[5]);
        damage_lua.set_field(lua, 5, behavior_add_names[add_input[5]]);
    }
    if (add_input[10] & 2u) {
        damage_lua.get_field(lua, -10002, "_ab_add_alternate");
        damage_lua.set_field(lua, 3, "behaviors");
    }
    if (add_input[10] & 4u) {
        unsigned length = add_length(lua, 6);
        if (length >= 4)
            fail("Original construction group exceeds fixture bound\n");
        add_instance(lua, 90);
        add_set_index(lua, 6, (int)length + 1);
    }
    add_instance(lua, add_input[11]);
    return 1;
}

static void add_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    for (uint32_t group = 0; group < 6; ++group) {
        add_group_addresses[group] = NULL;
        if (group == 4) {
            damage_lua.push_nil(lua);
        } else {
            damage_lua.create_table(lua, 4, 0);
            uint32_t count = group == 5 ? 1u : add_input[12];
            for (uint32_t slot = 0; slot < count; ++slot) {
                add_instance(lua, (group + 1u) * 10u + slot + 1u);
                add_set_index(lua, 1, (int)slot + 1);
            }
            add_group_addresses[group] = damage_lua.to_pointer(lua, 1);
        }
        damage_lua.set_field(lua, -10002, add_group_globals[group]);
    }
    damage_lua.create_table(lua, 0, (int)add_input[1]);
    for (uint32_t index = 0; index < add_input[2]; ++index) {
        uint32_t name = (add_input[3] + (add_input[4] ? 4u - index : index)) % 4u;
        uint32_t kind = name == add_input[5] ? add_input[6] : 2u;
        if (kind == 2)
            damage_lua.get_field(lua, -10002, add_group_globals[name]);
        else if (kind == 1)
            damage_lua.push_boolean(lua, 0);
        else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 1, behavior_add_names[name]);
    }
    damage_lua.set_field(lua, -10002, "_ab_add_primary");
    damage_lua.create_table(lua, 0, 0);
    for (uint32_t name = 0; name < 4; ++name) {
        damage_lua.get_field(lua, -10002, add_group_globals[5]);
        damage_lua.set_field(lua, 1, behavior_add_names[name]);
    }
    damage_lua.set_field(lua, -10002, "_ab_add_alternate");
    add_instance(lua, 101);
    damage_lua.get_field(lua, -10002, "_ab_add_primary");
    damage_lua.set_field(lua, 1, "behaviors");
    damage_lua.set_field(lua, -10002, "_ab_add_self");
    damage_lua.create_table(lua, 0, 4);
    for (uint32_t name = 0; name < 4; ++name) {
        damage_lua.create_table(lua, 0, 1);
        if (add_input[9] || name != add_input[5]) {
            damage_lua.create_table(lua, 0, 1);
            damage_function(lua, 3, "new", add_factory);
            damage_lua.set_field(lua, 2, "class");
        }
        damage_lua.set_field(lua, 1, behavior_add_names[name]);
    }
    damage_lua.set_field(lua, -10002, "BehaviorList");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "_ab_add_captured");
}

static void add_properties(void *lua) {
    switch (add_input[7]) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
        case 2:
            damage_lua.push_boolean(lua, add_input[7] == 2);
            break;
        case 3: {
            FloatBits number = {.bits = add_input[8]};
            damage_lua.push_number(lua, number.number);
            break;
        }
        case 4:
            damage_lua.push_string(lua, "payload");
            break;
        default:
            add_instance(lua, 103);
            break;
    }
}

static void add_snapshot(void *lua, int status) {
    damage_lua.set_top(lua, 0);
    add_output[0] = status != 0;
    damage_lua.get_field(lua, -10002, "_ab_add_self");
    damage_lua.get_field(lua, 1, "behaviors");
    const void *current = damage_lua.to_pointer(lua, 2);
    damage_lua.get_field(lua, -10002, "_ab_add_primary");
    add_output[6] = current == damage_lua.to_pointer(lua, 3) ? 1u : 2u;
    damage_lua.set_top(lua, 0);
    for (uint32_t map = 0; map < 2; ++map) {
        damage_lua.get_field(lua, -10002,
                             map ? "_ab_add_alternate" : "_ab_add_primary");
        for (uint32_t name = 0; name < 4; ++name) {
            damage_lua.get_field(lua, 1, behavior_add_names[name]);
            add_output[8 + map * 4 + name] = add_group_id(lua, 2);
            damage_lua.set_top(lua, 1);
        }
        damage_lua.push_nil(lua);
        uint32_t index = 0;
        while (add_next(lua, 1)) {
            if (index >= 4)
                fail("Original construction map exceeds fixture bound\n");
            for (uint32_t name = 0; name < 4; ++name)
                if (damage_named(lua, 2, behavior_add_names[name]))
                    add_output[40 + map * 4 + index] = name + 1u;
            ++index;
            damage_lua.set_top(lua, 2);
        }
        damage_lua.set_top(lua, 0);
    }
    add_output[7] = add_group_addresses[4] != NULL;
    for (uint32_t group = 0; group < 6; ++group) {
        damage_lua.get_field(lua, -10002, add_group_globals[group]);
        if (damage_lua.type(lua, 1) == 5) {
            for (uint32_t slot = 0; slot < 4; ++slot) {
                add_get_index(lua, 1, (int)slot + 1);
                if (damage_lua.type(lua, 2) == 5)
                    add_output[16 + group * 4 + slot] = add_id(lua, 2);
                else if (damage_lua.type(lua, 2) != 0)
                    fail("Unexpected construction group item type\n");
                damage_lua.set_top(lua, 1);
            }
        }
        damage_lua.set_top(lua, 0);
    }
    if (add_output[48]) {
        damage_lua.get_field(lua, -10002, "_ab_add_captured");
        add_output[49] = add_length(lua, 1);
        damage_lua.set_top(lua, 0);
    }
    print_words(add_output, 52);
}

static void probe_game_behavior_add(void) {
    initialize_damage_lua();
    add_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    add_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    add_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    add_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    add_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original behavior construction state\n");
    /* Collection of retained nil keys is a separately exposed map phase. Keep
     * that phase inactive for this ordinary construction/append comparison. */
    int (*collect)(void *, int, int) =
        (int (*)(void *, int, int))find_function("_Z6lua_gcP9lua_Stateii");
    if (collect(lua, 0, 0))
        fail("Cannot stop original constructor fixture collection\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    add_load_method(lua, "/data/local/tmp/ab520/camera/game-object-add-behavior.lua",
                    "_ab_add_method");
    add_load_method(lua, "/data/local/tmp/ab520/camera/behavior-utils-create.lua",
                    "_ab_add_create");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_add_create");
    damage_lua.set_field(lua, 1, "createBehavior");
    damage_lua.set_field(lua, -10002, "BehaviorUtils");
    semihost(4, "AB_ANDROID_GAME_BEHAVIOR_ADD_V1 2048\n");
    for (uint32_t sample = 0; sample < BEHAVIOR_ADD_CASES; ++sample) {
        behavior_add_case(sample, add_input);
        for (uint32_t index = 0; index < 52; ++index)
            add_output[index] = 0;
        print_words(add_input, 16);
        add_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_add_method");
        damage_lua.get_field(lua, -10002, "_ab_add_self");
        damage_lua.push_string(lua, behavior_add_names[add_input[5]]);
        add_properties(lua);
        int status = damage_lua.call(lua, 3, 0, 0);
        if (status && add_input[9]) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Unexpected original construction failure\n");
        }
        if (!status && !add_input[9])
            fail("Expected original missing behavior class failure\n");
        add_snapshot(lua, status);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB game behavior add reference complete\n");
}

#endif
