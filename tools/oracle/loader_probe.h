#ifndef AB_ORACLE_LOADER_H
#define AB_ORACLE_LOADER_H

static uint32_t ld_case, ld_count, ld_trace[24];
static void (*ld_raw_set)(void *, int, int);

static void ld_record(uint32_t kind) {
    if (ld_count == 24)
        fail("Original Loader trace exceeds bound\n");
    ld_trace[ld_count++] = kind;
}

static int ld_complete(void *lua) {
    if (damage_lua.get_top(lua))
        fail("Unexpected Loader completion arguments\n");
    ld_record(7);
    return 0;
}

static int ld_second(void *lua) {
    if (damage_lua.get_top(lua))
        fail("Unexpected Loader second-item arguments\n");
    ld_record(3);
    return 0;
}

static int ld_first(void *lua) {
    if (damage_lua.get_top(lua))
        fail("Unexpected Loader first-item arguments\n");
    ld_record(2);
    damage_lua.get_field(lua, -10002, "_ab_ld_self");
    damage_lua.get_field(lua, 1, "frame");
    if (damage_lua.to_number(lua, 2) != 1)
        fail("Original Loader executes items on unexpected frame\n");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, -10002, "_ab_ld_items");
    damage_lua.push_closure(lua, ld_second, 0);
    ld_raw_set(lua, 2, 2);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "load_items");
    return 0;
}

static int ld_log(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected Loader diagnostic arguments\n");
    if (damage_named(lua, 1, "no min display time"))
        ld_record(4);
    else if (damage_named(lua, 1, "min display time passed"))
        ld_record(8);
    else
        fail("Unexpected original Loader diagnostic\n");
    return 0;
}

static int ld_notify(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5)
        fail("Unexpected original Loader event arguments\n");
    damage_lua.get_field(lua, 2, "id");
    if (damage_named(lua, 3, "s")) {
        ld_record(1);
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "loadingPageAlpha");
        FloatBits alpha = {.number = damage_lua.to_number(lua, 3)};
        if (damage_lua.type(lua, 3) != 3 || alpha.bits != UINT32_C(0x3ecccccd))
            fail("Unexpected original Loader page alpha\n");
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, -10002, "_ab_ld_request");
        damage_lua.get_field(lua, -10002, "_ab_ld_items");
        damage_lua.set_field(lua, 3, "items");
    } else if (damage_named(lua, 3, "h")) {
        ld_record(5);
        if (ld_case == 4) {
            damage_lua.set_top(lua, 2);
            damage_lua.get_field(lua, -10002, "_ab_ld_self");
            damage_number(lua, 3, "frame", UINT32_C(0x41200000));
        }
    } else if (damage_named(lua, 3, "c")) {
        ld_record(6);
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, -10002, "_ab_ld_self");
        if (ld_case == 5)
            damage_lua.push_nil(lua);
        else
            damage_lua.push_closure(lua, ld_complete, 0);
        damage_lua.set_field(lua, 3, "completion_call");
    } else {
        fail("Unexpected original Loader notification ID\n");
    }
    return 0;
}

static void ld_call(void *lua, const char *name, int with_request) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, name);
    damage_lua.get_field(lua, -10002, "_ab_ld_self");
    if (with_request)
        damage_lua.get_field(lua, -10002, "_ab_ld_request");
    if (damage_lua.call(lua, with_request ? 2 : 1, 0, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original Loader method failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void ld_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 1, "loading");
    damage_number(lua, 1, "frame", 0);
    damage_lua.get_field(lua, -10002, "_ab_ld_finish");
    damage_lua.set_field(lua, 1, "finishLoading");
    damage_lua.set_field(lua, -10002, "_ab_ld_self");
    damage_lua.create_table(lua, 2, 0);
    damage_lua.push_closure(lua, ld_first, 0);
    ld_raw_set(lua, 1, 1);
    damage_lua.set_field(lua, -10002, "_ab_ld_items");
    damage_lua.create_table(lua, 0, 5);
    damage_lua.push_string(lua, "d");
    damage_lua.set_field(lua, 1, "id");
    if (ld_case == 1)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_nil(lua);
    damage_lua.set_field(lua, 1, "hidden");
    damage_number(lua, 1, "loadingPageAlpha", UINT32_C(0x3ecccccd));
    if (ld_case == 1 || ld_case == 4)
        damage_lua.push_number(lua, 0.5f);
    else if (ld_case == 2)
        damage_lua.push_number(lua, 0);
    else if (ld_case == 3)
        damage_lua.push_boolean(lua, 0);
    else
        damage_lua.push_nil(lua);
    damage_lua.set_field(lua, 1, "min_display_time");
    damage_lua.get_field(lua, -10002, "_ab_ld_items");
    damage_lua.set_field(lua, 1, "items");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "c");
    damage_lua.set_field(lua, 2, "id");
    damage_lua.set_field(lua, 1, "completion_event");
    damage_lua.set_field(lua, -10002, "_ab_ld_request");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "skipInput");
    damage_number(lua, -10002, "time", UINT32_C(0x42c80000));
    ld_call(lua, "_ab_ld_update", 0);
}

static void ld_value(void *lua, int table, const char *name, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 1) {
        output[1] = (uint32_t)damage_lua.to_boolean(lua, -1);
    } else if (output[0] != 0) {
        fail("Unexpected original Loader scalar type\n");
    }
    damage_lua.set_top(lua, top);
}

static void ld_snapshot(void *lua, uint32_t step) {
    uint32_t result[13] = {0}, temporary[2];
    damage_lua.get_field(lua, -10002, "_ab_ld_self");
    ld_value(lua, 1, "loading", result);
    ld_value(lua, 1, "frame", temporary);
    if (temporary[0] != 3)
        fail("Missing original Loader frame\n");
    result[2] = temporary[1];
    ld_value(lua, 1, "loading_frame", result + 3);
    ld_value(lua, 1, "min_display_time", result + 5);
    ld_value(lua, -10002, "skipInput", result + 7);
    damage_lua.get_field(lua, 1, "load_items");
    if (damage_lua.type(lua, 2) == 5) {
        damage_lua.get_field(lua, -10002, "_ab_ld_items");
        result[9] =
            damage_lua.to_pointer(lua, 2) == damage_lua.to_pointer(lua, 3) ? 1u : 2u;
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "completion_call");
    result[10] = damage_lua.type(lua, 2) == 6;
    damage_lua.set_top(lua, 0);
    result[11] = ld_count;
    ld_value(lua, -10002, "time", temporary);
    result[12] = temporary[1];
    uint32_t header[2] = {ld_case, step};
    print_words(header, 2);
    print_words(result, 13);
    if (ld_count)
        print_words(ld_trace, ld_count);
}

static void probe_loader(void) {
    initialize_damage_lua();
    ld_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original Loader state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    load_selected_function(lua, "/data/local/tmp/ab520/loader/event.lua",
                           "_ab_ld_event");
    load_selected_function(lua, "/data/local/tmp/ab520/loader/finish.lua",
                           "_ab_ld_finish");
    load_selected_function(lua, "/data/local/tmp/ab520/loader/update.lua",
                           "_ab_ld_update");
    damage_function(lua, -10002, "print", ld_log);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "notify", ld_notify);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 3);
    const char *names[] = {"EID_DO_LOADING", "EID_SHOW_LOADING_PAGE",
                           "EID_HIDE_LOADING_PAGE"};
    const char *values[] = {"d", "s", "h"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.push_string(lua, values[index]);
        damage_lua.set_field(lua, 1, names[index]);
    }
    damage_lua.set_field(lua, -10002, "events");
    semihost(4, "AB_ANDROID_LOADER_V1 6 7\n");
    for (ld_case = 0; ld_case < 6; ++ld_case) {
        ld_prepare(lua);
        for (uint32_t step = 0; step < 7; ++step) {
            ld_count = 0;
            if (step == 1) {
                ld_call(lua, "_ab_ld_event", 1);
            } else if (step >= 2) {
                if (step == 4)
                    damage_number(lua, -10002, "time", UINT32_C(0x42c88000));
                if (step >= 5)
                    damage_number(lua, -10002, "time", UINT32_C(0x42c90000));
                ld_call(lua, "_ab_ld_update", 0);
            }
            ld_snapshot(lua, step);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB loader reference complete\n");
}

#endif
