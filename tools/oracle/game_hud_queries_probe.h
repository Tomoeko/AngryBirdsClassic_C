#ifndef AB_ORACLE_GAME_HUD_QUERIES_H
#define AB_ORACLE_GAME_HUD_QUERIES_H

/* Execute the original GameHud methods. Settings/level/common query replies are
 * controlled boundaries; no constructor, complete class or GPU parity claim. */
static uint32_t hud_queries_input[13];

static void hud_queries_value(void *lua, uint32_t code) {
    switch (code) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
        case 2:
            damage_lua.push_boolean(lua, code == 2);
            break;
        case 3:
        case 4:
            damage_lua.push_number(lua, code == 3 ? 0 : -1);
            break;
        default:
            damage_lua.push_string(lua, "");
            break;
    }
}

static void hud_queries_field(void *lua, int table, const char *name, unsigned index) {
    hud_queries_value(lua, hud_queries_input[index]);
    damage_lua.set_field(lua, table, name);
}

static int hud_queries_ending(void *lua) {
    if (hud_queries_input[2] < 2)
        damage_lua.push_boolean(lua, 0);
    else
        hud_queries_value(lua, hud_queries_input[2]);
    return 1;
}

static int hud_queries_purchased(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5)
        fail("Unexpected original HUD RMF query arguments\n");
    if (!hud_queries_input[10])
        damage_lua.push_boolean(lua, 0);
    else
        hud_queries_value(lua, hud_queries_input[10]);
    return 1;
}

static int hud_queries_unlocked(void *lua) {
    if (damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 2, "shockwavePowerupUnlocked"))
        fail("Unexpected original HUD synced query arguments\n");
    damage_lua.push_boolean(lua, hud_queries_input[11] >= 2);
    return 1;
}

static int hud_queries_level(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "Level1"))
        fail("Unexpected original HUD level query arguments\n");
    damage_lua.create_table(lua, 0, 0);
    for (unsigned index = 0; index < 3; ++index)
        damage_lua.push_number(lua, 1);
    return 4;
}

static int hud_queries_level_type(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "Level1"))
        fail("Unexpected original HUD level-type query arguments\n");
    damage_lua.push_string(lua, hud_queries_input[4] >= 2 ? "golden_egg" : "basic");
    return 1;
}

static void hud_queries_case(void *lua) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    const char *const names[] = {"g_powerups_enabled", "eagleBaitLaunched",
                                 "g_level_completing",
                                 "g_enable_powerups_in_golden_egg_levels"};
    for (unsigned index = 0; index < 4; ++index)
        hud_queries_field(lua, 1, names[index], index);
    damage_lua.push_string(lua, "Level1");
    damage_lua.set_field(lua, 1, "levelName");
    if (hud_queries_input[6] < 2)
        hud_queries_value(lua, hud_queries_input[6]);
    else
        damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, 1, "currentEpisode");
    const char *const models[] = {"android", "iphone", "ipad"};
    damage_lua.push_string(lua, models[hud_queries_input[12]]);
    damage_lua.set_field(lua, 1, "deviceModel");
    damage_function(lua, 1, "isLevelEnding", hud_queries_ending);
    damage_function(lua, 1, "getLevelById", hud_queries_level);
    damage_function(lua, 1, "getLevelType", hud_queries_level_type);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 2, "isRedsMightyFeathersPurchased", hud_queries_purchased);
    damage_function(lua, 2, "getSyncedFlag", hud_queries_unlocked);
    damage_lua.set_field(lua, 1, "settingsWrapper");
    damage_lua.create_table(lua, 1, 0);
    damage_lua.create_table(lua, 0, 3);
    hud_queries_field(lua, 3, "powerup_button_disabled", 7);
    hud_queries_field(lua, 3, "rmf_button_enabled", 8);
    damage_lua.create_table(lua, 1, 0);
    damage_lua.create_table(lua, 0, 2);
    hud_queries_field(lua, 5, "golden_egg_end_screen", 4);
    hud_queries_field(lua, 5, "rmf_button_enabled", 9);
    raw_set(lua, 4, 1);
    damage_lua.set_field(lua, 3, "pages");
    raw_set(lua, 2, 1);
    damage_lua.set_field(lua, 1, "g_episodes");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 3);
    hud_queries_field(lua, 1, "hidePowerups", 5);
    damage_lua.get_field(lua, -10002, "GameHud");
    const char *const methods[] = {"isHudHidingNeeded", "powerupButtonShouldBeVisible"};
    for (unsigned index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, 2, methods[index]);
        damage_lua.set_field(lua, 1, methods[index]);
    }
    damage_lua.set_top(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_hud_queries");
}

static void probe_game_hud_queries(void *lua) {
    uint32_t random = UINT32_C(0x52022112);
    const char *const methods[] = {"powerupButtonShouldBeVisible",
                                   "redsMightyFeatherButtonShouldBeVisible",
                                   "shouldShockwaveButtonBeVisible"};
    semihost(4, "AB_ANDROID_GAME_HUD_QUERIES_V1 4096\n");
    for (unsigned item = 0; item < 4096; ++item) {
        for (unsigned field = 0; field < 12; ++field)
            hud_queries_input[field] = random_word(&random) % 6;
        hud_queries_input[12] = item % 3;
        hud_queries_case(lua);
        uint32_t output[6] = {0};
        for (unsigned method = 0; method < 3; ++method) {
            damage_lua.get_field(lua, -10002, "GameHud");
            damage_lua.get_field(lua, 1, methods[method]);
            damage_lua.get_field(lua, -10002, "_ab_hud_queries");
            if (damage_lua.call(lua, 1, 1, 0)) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
                fail(message ? message : "Original HUD predicate failed\n");
            }
            int type = damage_lua.type(lua, -1);
            output[method * 2] = (uint32_t)type;
            if (type == 1)
                output[method * 2 + 1] = (uint32_t)damage_lua.to_boolean(lua, -1);
            else if (type == 3) {
                FloatBits value = {.number = damage_lua.to_number(lua, -1)};
                output[method * 2 + 1] = value.bits;
            } else if (type != 0)
                fail("Unexpected original HUD predicate result type\n");
            damage_lua.set_top(lua, 0);
        }
        print_words(hud_queries_input, 13);
        print_words(output, 6);
    }
    semihost(4, "AB game HUD queries reference complete\n");
}

#endif
