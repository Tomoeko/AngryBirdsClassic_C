#ifndef AB_ORACLE_PROCESS_MANAGER_H
#define AB_ORACLE_PROCESS_MANAGER_H

static uint32_t pm_case, pm_count, pm_rows[64][5], pm_calls[2];
static unsigned (*pm_length)(void *, int);
static void (*pm_raw_get)(void *, int, int);

static uint32_t *pm_record(unsigned kind) {
    if (pm_count == 64)
        fail("Original process trace exceeds bound\n");
    uint32_t *row = pm_rows[pm_count++];
    for (size_t index = 0; index < 5; ++index)
        row[index] = 0;
    row[0] = kind;
    return row;
}

static void pm_action_table(void *lua, unsigned index) {
    damage_lua.get_field(lua, -10002, index ? "_ab_pm_b" : "_ab_pm_a");
}

static void pm_call(void *lua, const char *method, int argument) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "ProcessManager");
    damage_lua.get_field(lua, top + 1, method);
    damage_lua.get_field(lua, -10002, "_ab_pm_self");
    if (argument == 1)
        pm_action_table(lua, 0);
    else if (argument == 2)
        pm_action_table(lua, 1);
    else if (argument == 3)
        damage_lua.push_number(lua, 0.25f);
    else if (argument == 4)
        damage_lua.push_string(lua, "first");
    if (damage_lua.call(lua, argument ? 2 : 1, 0, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original process method failed\n");
    }
    damage_lua.set_top(lua, top);
}

static int pm_action(void *lua) {
    unsigned index = (unsigned)damage_lua.to_number(lua, -10003);
    if (index > 1 || damage_lua.get_top(lua) != 5 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.to_number(lua, 1) != (float)index || damage_lua.type(lua, 5) != 1 ||
        !damage_lua.to_boolean(lua, 5))
        fail("Unexpected original process action arguments\n");
    uint32_t *row = pm_record(index * 10 + 1);
    for (unsigned argument = 2; argument <= 4; ++argument) {
        if (damage_lua.type(lua, (int)argument) != 3)
            fail("Missing original process numeric argument\n");
        FloatBits value = {.number = damage_lua.to_number(lua, (int)argument)};
        row[argument - 1] = value.bits;
    }
    FloatBits item = {.number = (float)index};
    row[4] = item.bits;
    uint32_t call = pm_calls[index]++;
    if (pm_case == 7 && !index && !call)
        pm_call(lua, "insertAction", 2);
    if (pm_case == 6) {
        pm_action_table(lua, index);
        damage_number(lua, 6, "duration", UINT32_C(0x3e800000));
        damage_lua.set_top(lua, 5);
    }
    return 0;
}

static int pm_finish(void *lua) {
    unsigned index = (unsigned)damage_lua.to_number(lua, -10003);
    if (index > 1 || damage_lua.get_top(lua))
        fail("Unexpected original process onFinish arguments\n");
    (void)pm_record(index * 10 + 2);
    if (pm_case == 6) {
        pm_action_table(lua, index);
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "finished");
        damage_lua.set_top(lua, 0);
    }
    return 0;
}

static int pm_log(void *lua) {
    unsigned length = 0;
    const char *text = damage_lua.to_string(lua, 1, &length);
    if (damage_lua.get_top(lua) != 1 || !text || !length || length > 128 ||
        (text[length - 1] != 'a' && text[length - 1] != 'b'))
        fail("Unexpected original process log\n");
    (void)pm_record(text[length - 1] == 'a' ? 3u : 13u);
    return 0;
}

static int pm_notify(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5)
        fail("Unexpected original process notification\n");
    damage_lua.get_field(lua, 2, "id");
    if (!damage_named(lua, 3, "EID_PROCESS_FINISHED"))
        fail("Unexpected original process event ID\n");
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "target");
    uint32_t array = damage_lua.type(lua, 3) == 0;
    if (array) {
        damage_lua.set_top(lua, 2);
        pm_raw_get(lua, 2, 1);
    }
    if (damage_lua.type(lua, 3) != 5)
        fail("Missing original process event action\n");
    damage_lua.get_field(lua, 3, "name");
    unsigned index = damage_named(lua, 4, "a") ? 0u : 1u;
    if (index && !damage_named(lua, 4, "b"))
        fail("Unknown original process action\n");
    uint32_t *row = pm_record(index * 10 + 4);
    row[4] = array;
    return 0;
}

static void pm_bind(void *lua, int table, const char *name, unsigned index,
                    int (*method)(void *)) {
    damage_lua.push_number(lua, (float)index);
    damage_lua.push_closure(lua, method, 1);
    damage_lua.set_field(lua, table, name);
}

static void pm_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "ProcessManager");
    damage_lua.get_field(lua, 2, "reset");
    damage_lua.set_field(lua, 1, "reset");
    damage_lua.set_top(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_pm_self");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "skipInput");
    for (unsigned index = 0; index < 2; ++index) {
        pm_calls[index] = 0;
        damage_lua.create_table(lua, 0, 9);
        damage_lua.push_string(lua, index ? "second" : "first");
        damage_lua.set_field(lua, 1, "id");
        damage_lua.push_string(lua, index ? "b" : "a");
        damage_lua.set_field(lua, 1, "name");
        damage_number(lua, 1, "start",
                      pm_case == 5 && !index ? UINT32_C(0x3f800000)
                                             : UINT32_C(0x3e800000));
        damage_number(lua, 1, "duration",
                      pm_case == 5 && !index ? UINT32_C(0xbf000000)
                                             : UINT32_C(0x3f000000));
        if (pm_case == 2)
            damage_lua.push_nil(lua);
        else if (pm_case == 4)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_boolean(lua, pm_case == 3);
        damage_lua.set_field(lua, 1, "finished");
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 1, "item");
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "meta");
        pm_bind(lua, 1, "action", index, pm_action);
        pm_bind(lua, 1, "onFinish", index, pm_finish);
        damage_lua.set_field(lua, -10002, index ? "_ab_pm_b" : "_ab_pm_a");
    }
    pm_call(lua, "init", 0);
}

static void pm_value(void *lua, int table, const char *name, uint32_t words[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    words[0] = (uint32_t)damage_lua.type(lua, -1);
    words[1] = 0;
    if (words[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        words[1] = value.bits;
    } else if (words[0] == 1) {
        words[1] = (uint32_t)damage_lua.to_boolean(lua, -1);
    } else if (words[0] != 0) {
        fail("Unsupported original process state field\n");
    }
    damage_lua.set_top(lua, top);
}

static void pm_snapshot(void *lua, uint32_t step) {
    uint32_t result[9] = {0}, temporary[2];
    damage_lua.get_field(lua, -10002, "_ab_pm_self");
    pm_value(lua, 1, "running", result);
    pm_value(lua, 1, "elapsed", temporary);
    if (temporary[0] != 3)
        fail("Missing original process elapsed\n");
    result[2] = temporary[1];
    pm_value(lua, -10002, "skipInput", result + 3);
    damage_lua.get_field(lua, 1, "actions");
    result[5] = pm_length(lua, 2);
    damage_lua.set_top(lua, 0);
    pm_action_table(lua, 0);
    pm_value(lua, 1, "finished", result + 6);
    damage_lua.set_top(lua, 0);
    result[8] = pm_count;
    uint32_t header[2] = {pm_case, step};
    print_words(header, 2);
    print_words(result, 9);
    for (uint32_t index = 0; index < pm_count; ++index)
        print_words(pm_rows[index], 5);
}

static void probe_process_manager(void) {
    initialize_damage_lua();
    pm_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    pm_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original process state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/process-manager/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/process-manager/manager.lua");
    damage_function(lua, -10002, "loginfo", pm_log);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "notify", pm_notify);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "EID_PROCESS_FINISHED");
    damage_lua.set_field(lua, 1, "EID_PROCESS_FINISHED");
    damage_lua.set_field(lua, -10002, "events");
    semihost(4, "AB_ANDROID_PROCESS_MANAGER_V1 8 9\n");
    for (pm_case = 0; pm_case < 8; ++pm_case) {
        pm_prepare(lua);
        for (uint32_t step = 0; step < 9; ++step) {
            pm_count = 0;
            if (step == 1) {
                pm_call(lua, "insertAction", 1);
            } else if (step == 2) {
                if (pm_case) {
                    pm_call(lua, "start", 0);
                } else {
                    damage_lua.get_field(lua, -10002, "_ab_pm_self");
                    damage_number(lua, 1, "running", 0);
                    damage_lua.set_top(lua, 0);
                }
            } else if (step >= 3 && step <= 7) {
                pm_call(lua, pm_case == 7 && step == 5 ? "resetProcess" : "update",
                        pm_case == 7 && step == 5 ? 4 : 3);
            } else if (step == 8) {
                pm_call(lua, "reset", 0);
            }
            pm_snapshot(lua, step);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB process manager reference complete\n");
}

#endif
