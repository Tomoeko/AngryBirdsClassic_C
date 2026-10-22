#ifndef AB_ORACLE_MENU_POINTER_H
#define AB_ORACLE_MENU_POINTER_H

static void (*pointer_push_value)(void *, int);
static void (*pointer_raw_set)(void *, int, int);
static void (*pointer_raw_get)(void *, int, int);
static int (*pointer_next)(void *, int);
static uint32_t pointer_input[10], pointer_calls[16][4];
static uint32_t pointer_call_count, pointer_copy_count;

static void pointer_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original menu pointer call failed\n");
    }
}

static int pointer_copy(void *lua) {
    ++pointer_copy_count;
    damage_lua.get_field(lua, -10002, "pointer-copy");
    pointer_push_value(lua, 1);
    pointer_call(lua, 1, 1);
    return 1;
}

static int pointer_event(void *lua) {
    static const char *const events[] = {"LPRESS", "LHOLD", "LRELEASE", "WHEEL"};
    uint32_t phase = 4;
    for (uint32_t index = 0; index < 4; ++index) {
        if (damage_named(lua, 2, events[index])) {
            phase = index;
        }
    }
    damage_lua.get_field(lua, 1, "_ab_slot");
    uint32_t slot = (uint32_t)damage_lua.to_number(lua, -1);
    if (phase >= 4 || slot < 1 || slot > 4 || pointer_call_count >= 16) {
        fail("Unexpected menu pointer callback\n");
    }
    uint32_t *record = pointer_calls[pointer_call_count++];
    record[0] = phase;
    record[1] = slot;
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.number = damage_lua.to_number(lua, (int)axis + 3)};
        record[2 + axis] = value.bits;
    }
    uint32_t bit = 1u << (phase * 4 + slot - 1);
    damage_lua.push_boolean(lua, (pointer_input[2] & bit) != 0);
    damage_lua.push_nil(lua);
    return 2;
}

static void pointer_prepare(void *lua, uint32_t touches[8][3]) {
    pointer_call_count = pointer_copy_count = 0;
    damage_lua.create_table(lua, 0, 8);
    int manager = 1;
    damage_lua.push_boolean(lua, (int)(pointer_input[0] & 16));
    damage_lua.set_field(lua, manager, "allowInput");
    damage_lua.get_field(lua, -10002, "pointer-pointer");
    damage_lua.set_field(lua, manager, "delegatePointerEvent");
    damage_lua.create_table(lua, 4, 0);
    for (int index = 1; index <= 4; ++index) {
        damage_lua.push_number(lua, (float)pointer_input[5 + (size_t)index]);
        pointer_raw_set(lua, 2, index);
    }
    damage_lua.set_field(lua, manager, "input_order");
    damage_lua.create_table(lua, 4, 0);
    for (unsigned index = 0; index < 4; ++index) {
        if (!(pointer_input[1] & (1u << index))) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_number(lua, (float)(index + 1));
            damage_lua.set_field(lua, 3, "_ab_slot");
            damage_function(lua, 3, "onPointerEvent", pointer_event);
            pointer_raw_set(lua, 2, (int)index + 1);
        }
    }
    damage_lua.set_field(lua, manager, "menus");
    damage_lua.set_field(lua, -10002, "_ab_pointer_manager");
    damage_lua.create_table(lua, 0, 8);
    const char *fields[] = {"keyPressed", "keyHold", "keyReleased"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_boolean(lua, (int)(pointer_input[0] & (1u << index)));
        damage_lua.set_field(lua, 2, "LBUTTON");
        damage_lua.set_field(lua, 1, fields[index]);
    }
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 2, "x", pointer_input[3]);
    damage_number(lua, 2, "y", pointer_input[4]);
    damage_lua.push_boolean(lua, (int)(pointer_input[0] & 8));
    damage_lua.set_field(lua, 2, "wheelTriggered");
    damage_lua.set_field(lua, 1, "cursor");
    damage_lua.create_table(lua, 0, (int)pointer_input[5]);
    for (uint32_t index = 0; index < pointer_input[5]; ++index) {
        FloatBits id = {.bits = touches[index][0]};
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 3, "x", touches[index][1]);
        damage_number(lua, 3, "y", touches[index][2]);
        pointer_raw_set(lua, 2, (int)id.number);
    }
    damage_lua.set_field(lua, 1, "touches");
    damage_function(lua, 1, "deepCopy", pointer_copy);
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void pointer_run(void *lua, uint32_t touches[8][3]) {
    const char *methods[] = {"pointer-reset", "pointer-clicks"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, methods[index]);
        damage_lua.get_field(lua, -10002, "_ab_pointer_manager");
        pointer_call(lua, 1, 0);
    }
    damage_lua.get_field(lua, -10002, "_ab_pointer_manager");
    damage_lua.get_field(lua, 1, "filteredInput");
    uint32_t output[5] = {0};
    const char *fields[] = {"keyPressed", "keyHold", "keyReleased"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, 2, fields[field]);
        damage_lua.get_field(lua, 3, "LBUTTON");
        if (damage_lua.type(lua, 4) == 1) {
            output[0] |= 1u << field;
            if (damage_lua.to_boolean(lua, 4)) {
                output[1] |= 1u << field;
            }
        }
        damage_lua.set_top(lua, 2);
    }
    damage_lua.get_field(lua, 2, "wheelTriggered");
    if (damage_lua.type(lua, 3) != 1) {
        fail("Missing original filtered wheel field\n");
    }
    output[0] |= 8;
    output[1] |= damage_lua.to_boolean(lua, 3) ? 8u : 0u;
    output[2] = pointer_copy_count;
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "filteredTouches");
    damage_lua.push_nil(lua);
    while (pointer_next(lua, 3)) {
        ++output[3];
        damage_lua.set_top(lua, 4);
    }
    damage_lua.set_top(lua, 2);
    output[4] = pointer_call_count;
    if (pointer_copy_count > 1) {
        fail("Unexpected recursive root-copy boundary\n");
    }
    print_words(output, 5);
    for (uint32_t index = 0; index < pointer_call_count; ++index) {
        print_words(pointer_calls[index], 4);
    }
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "filteredTouches");
    for (uint32_t index = 0; index < pointer_input[5]; ++index) {
        FloatBits id = {.bits = touches[index][0]};
        pointer_raw_get(lua, 3, (int)id.number);
        uint32_t member[3] = {0};
        if (damage_lua.type(lua, 4) == 5) {
            member[0] = 1;
            const char *axes[] = {"x", "y"};
            for (size_t axis = 0; axis < 2; ++axis) {
                damage_lua.get_field(lua, 4, axes[axis]);
                FloatBits value = {.number = damage_lua.to_number(lua, 5)};
                member[1 + axis] = value.bits;
                damage_lua.set_top(lua, 4);
            }
        }
        print_words(member, 3);
        damage_lua.set_top(lua, 3);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_menu_pointer(void) {
    initialize_damage_lua();
    pointer_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    pointer_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    pointer_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    pointer_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original menu pointer state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {"/data/local/tmp/ab520/input/pointer-reset-oracle.lua",
                           "/data/local/tmp/ab520/input/pointer-pointer-oracle.lua",
                           "/data/local/tmp/ab520/input/pointer-clicks-oracle.lua",
                           "/data/local/tmp/ab520/input/pointer-copy-oracle.lua"};
    const char *names[] = {"pointer-reset", "pointer-pointer", "pointer-clicks",
                           "pointer-copy"};
    for (size_t index = 0; index < 4; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.get_field(lua, -10002, "pointer-copy");
    damage_lua.set_field(lua, -10002, "deepCopy");
    uint32_t random = UINT32_C(0x520fa411);
    semihost(4, "AB_ANDROID_MENU_POINTER_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        pointer_input[0] = (uint32_t)(item & 31);
        pointer_input[1] = item < 64 ? 0 : random_word(&random) >> 28;
        pointer_input[2] =
            item < 64 ? (item & 32 ? 65535u : 0u) : random_word(&random) >> 16;
        for (size_t axis = 0; axis < 2; ++axis) {
            FloatBits cursor = {
                .number = (float)((int)(random_word(&random) % 8192) - 4096) / 16.0f};
            pointer_input[3 + axis] = cursor.bits;
        }
        pointer_input[5] = (uint32_t)(item % 9);
        for (uint32_t index = 0; index < 4; ++index) {
            pointer_input[6 + index] = index + 1;
        }
        for (uint32_t end = 4; end > 1; --end) {
            uint32_t other = (random_word(&random) >> 16) % end;
            uint32_t value = pointer_input[5 + end];
            pointer_input[5 + end] = pointer_input[6 + other];
            pointer_input[6 + other] = value;
        }
        uint32_t touches[8][3] = {{0}};
        for (uint32_t index = 0; index < pointer_input[5]; ++index) {
            FloatBits id = {.number = (float)((int)index * 17 - 34)};
            touches[index][0] = id.bits;
            for (size_t axis = 0; axis < 2; ++axis) {
                touches[index][axis + 1] = random_float(&random, 118, 16, 1);
            }
        }
        print_words(pointer_input, 10);
        for (uint32_t index = 0; index < pointer_input[5]; ++index) {
            print_words(touches[index], 3);
        }
        pointer_prepare(lua, touches);
        pointer_run(lua, touches);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB menu pointer reference complete\n");
}

#endif
