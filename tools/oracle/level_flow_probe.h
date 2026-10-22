#ifndef AB_ORACLE_LEVEL_FLOW_H
#define AB_ORACLE_LEVEL_FLOW_H

static uint32_t flow_input[20];
static uint32_t flow_query_count, flow_notify_count, flow_hook_count;
static uint32_t flow_notify_observed, flow_hook_observed;

static void flow_kind(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, "truthy");
    }
}

static void flow_global_kind(void *lua, const char *name, uint32_t kind) {
    flow_kind(lua, kind);
    damage_lua.set_field(lua, -10002, name);
}

static uint32_t flow_flag(void *lua, const char *name, int literal) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    uint32_t result = (uint32_t)damage_lua.to_boolean(lua, -1);
    if (literal && damage_lua.type(lua, -1) != 1) {
        result = 0;
    }
    damage_lua.set_top(lua, top);
    return result;
}

static int flow_query(void *lua) {
    if (damage_lua.get_top(lua) != 0) {
        fail("Unexpected original completion query arguments\n");
    }
    ++flow_query_count;
    if (flow_input[9] & 16) {
        flow_global_kind(lua, "g_prevent_level_ending", 2);
        flow_global_kind(lua, "eagleBaitLaunched", 2);
        flow_global_kind(lua, "eagleTimer", 0);
        flow_global_kind(lua, "levelTimeout", flow_input[9] & 2 ? 2 : 1);
        flow_global_kind(lua, "hasMovingObjects", flow_input[9] & 4 ? 2 : 1);
        damage_lua.get_field(lua, -10002, "objects");
        flow_kind(lua, flow_input[9] & 1 ? 2 : 1);
        damage_lua.set_field(lua, 1, "doNotTimeout");
        flow_kind(lua, flow_input[9] & 8 ? 2 : 1);
        damage_lua.set_field(lua, 1, "doNotWaitForMovingObjects");
        damage_lua.set_top(lua, 0);
    }
    flow_kind(lua, flow_input[8]);
    return 1;
}

static int flow_notify(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Unexpected original goals notification arguments\n");
    }
    damage_lua.get_field(lua, 2, "id");
    if (!damage_named(lua, -1, "cleared")) {
        fail("Unexpected original goals notification identifier\n");
    }
    ++flow_notify_count;
    flow_notify_observed = flow_flag(lua, "g_level_goals_cleared_notification_sent", 0);
    flow_global_kind(lua, "g_level_goals_cleared_notification_sent", 1);
    flow_global_kind(lua, "g_disable_level_goal_destruction_check", 2);
    damage_lua.get_field(lua, -10002, "levelGoals");
    damage_lua.get_field(lua, -1, "first");
    flow_kind(lua, 2);
    damage_lua.set_field(lua, -2, "levelGoal");
    return 0;
}

static int flow_before_hook(void *lua) {
    if (damage_lua.get_top(lua) != 0) {
        fail("Unexpected original before-ending hook arguments\n");
    }
    ++flow_hook_count;
    flow_hook_observed = flow_flag(lua, "g_before_level_end_script_called", 0);
    if (flow_input[19] & 1) {
        damage_number(lua, -10002, "levelCompleteTimer", flow_input[18]);
    }
    if (flow_input[19] & 2) {
        flow_global_kind(lua, "g_before_level_end_script_called", 1);
    }
    return 0;
}

static void flow_call(void *lua, const char *name, int arguments, int results) {
    damage_lua.get_field(lua, -10002, name);
    if (arguments) {
        FloatBits seconds = {.bits = flow_input[16]};
        damage_lua.push_number(lua, seconds.number);
    }
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original level flow function failed\n");
    }
}

static void flow_prepare_completion(void *lua) {
    flow_global_kind(lua, "g_prevent_level_ending", flow_input[0]);
    flow_global_kind(lua, "eagleBaitLaunched", flow_input[1]);
    if (flow_input[2]) {
        damage_number(lua, -10002, "eagleTimer", flow_input[3]);
    } else {
        flow_global_kind(lua, "eagleTimer", 0);
    }
    flow_global_kind(lua, "levelTimeout", flow_input[5]);
    flow_global_kind(lua, "hasMovingObjects", flow_input[6]);
    damage_lua.create_table(lua, 0, 2);
    flow_kind(lua, flow_input[4]);
    damage_lua.set_field(lua, 1, "doNotTimeout");
    flow_kind(lua, flow_input[7]);
    damage_lua.set_field(lua, 1, "doNotWaitForMovingObjects");
    damage_lua.set_field(lua, -10002, "objects");
}

static void flow_prepare_goals(void *lua) {
    flow_global_kind(lua, "g_disable_level_goal_destruction_check", flow_input[10]);
    flow_global_kind(lua, "g_level_goals_cleared_notification_sent", flow_input[11]);
    damage_lua.create_table(lua, 0, 2);
    const char *names[] = {"first", "second"};
    for (size_t goal = 0; goal < 2; ++goal) {
        damage_lua.create_table(lua, 0, 1);
        flow_kind(lua, flow_input[12 + goal]);
        damage_lua.set_field(lua, 2, "levelGoal");
        damage_lua.set_field(lua, 1, names[goal]);
    }
    damage_lua.set_field(lua, -10002, "levelGoals");
}

static void flow_prepare_before(void *lua) {
    flow_global_kind(lua, "g_before_level_end_script_called", flow_input[14]);
    damage_number(lua, -10002, "levelCompleteTimer", flow_input[15]);
    damage_lua.create_table(lua, 0, 1);
    if (flow_input[17]) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 2, "onBeforeLevelEnding", flow_before_hook);
        damage_lua.set_field(lua, 1, "level");
    }
    damage_lua.set_field(lua, -10002, "g_level_scripts");
}

static void probe_level_flow(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original level flow reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua,
                           "/data/local/tmp/ab520/flow/check-level-complete-oracle.lua",
                           "_ab_complete");
    load_selected_function(
        lua, "/data/local/tmp/ab520/flow/check-level-goals-oracle.lua", "_ab_goals");
    load_selected_function(
        lua, "/data/local/tmp/ab520/flow/before-level-ending-oracle.lua", "_ab_before");
    damage_function(lua, -10002, "checkLevelGoalsDestroyed", flow_query);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "notify", flow_notify);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "cleared");
    damage_lua.set_field(lua, 1, "EID_LEVEL_GOALS_CLEARED");
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.push_string(lua, "level");
    damage_lua.set_field(lua, -10002, "levelName");
    uint32_t random = UINT32_C(0x520f10a5);
    semihost(4, "AB_ANDROID_LEVEL_FLOW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 20; ++field) {
            flow_input[field] = random_word(&random) % 5;
        }
        flow_input[0] = (uint32_t)(item / 512) % 5;
        flow_input[1] = (uint32_t)(item / 64) % 5;
        flow_input[2] &= 1;
        FloatBits timer = {.number = (float)((int)(item % 7) - 3) * 0.1f};
        FloatBits seconds = {.number = (float)(item % 19) / 100.0f};
        flow_input[3] = timer.bits;
        flow_input[9] = random_word(&random) & 31;
        flow_input[10] = (uint32_t)(item / 1000) % 5;
        flow_input[15] = random_float(&random, 0, 130, 1);
        flow_input[16] = seconds.bits;
        flow_input[17] &= 1;
        flow_input[18] = random_float(&random, 0, 130, 1);
        flow_input[19] &= 3;
        if (item < 32) {
            flow_input[3] = item & 1 ? UINT32_C(0x80000000) : 0;
            flow_input[15] = flow_input[3];
            flow_input[16] = 0;
        }
        flow_query_count = flow_notify_count = flow_hook_count = 0;
        flow_notify_observed = flow_hook_observed = 0;
        print_words(flow_input, 20);
        flow_prepare_completion(lua);
        flow_call(lua, "_ab_complete", 0, 1);
        uint32_t output[17] = {0};
        output[0] = (uint32_t)damage_lua.to_boolean(lua, -1);
        output[1] = flow_query_count;
        damage_lua.set_top(lua, 0);
        output[2] = flow_flag(lua, "g_prevent_level_ending", 0);
        output[3] = flow_flag(lua, "eagleBaitLaunched", 1);
        damage_lua.get_field(lua, -10002, "eagleTimer");
        output[4] = damage_lua.type(lua, -1) != 0;
        damage_lua.set_top(lua, 0);
        output[6] = flow_flag(lua, "levelTimeout", 0);
        output[7] = flow_flag(lua, "hasMovingObjects", 0);
        damage_lua.get_field(lua, -10002, "objects");
        damage_lua.get_field(lua, 1, "doNotTimeout");
        output[5] = (uint32_t)damage_lua.to_boolean(lua, -1);
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "doNotWaitForMovingObjects");
        output[8] = damage_lua.type(lua, -1) == 1 && damage_lua.to_boolean(lua, -1);
        damage_lua.set_top(lua, 0);
        flow_prepare_goals(lua);
        flow_call(lua, "_ab_goals", 0, 1);
        output[9] = (uint32_t)damage_lua.to_boolean(lua, -1);
        damage_lua.set_top(lua, 0);
        output[10] = flow_flag(lua, "g_level_goals_cleared_notification_sent", 0);
        output[11] = flow_notify_count;
        output[12] = flow_notify_observed;
        flow_prepare_before(lua);
        flow_call(lua, "_ab_before", 1, 0);
        output[13] = flow_flag(lua, "g_before_level_end_script_called", 0);
        damage_lua.get_field(lua, -10002, "levelCompleteTimer");
        FloatBits remaining = {.number = damage_lua.to_number(lua, -1)};
        output[14] = remaining.bits;
        output[15] = flow_hook_count;
        output[16] = flow_hook_observed;
        damage_lua.set_top(lua, 0);
        print_words(output, 17);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level flow reference complete\n");
}

#endif
