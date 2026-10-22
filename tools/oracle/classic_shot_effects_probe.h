#ifndef AB_ORACLE_CLASSIC_SHOT_EFFECTS_H
#define AB_ORACLE_CLASSIC_SHOT_EFFECTS_H

static void (*se_push_value)(void *, int);

static void se_encode(void *lua, int table, const char *name, uint32_t output[2]) {
    damage_lua.get_field(lua, table, name);
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1)
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    else if (output[0] == 3) {
        FloatBits number = {.number = damage_lua.to_number(lua, -1)};
        output[1] = number.bits;
    } else if (output[0] != 0 && !damage_named(lua, -1, ""))
        fail("Unexpected ordinary shot effect field type\n");
    damage_lua.set_top(lua, table);
}

static void probe_classic_shot_effects(void) {
    void *lua = open_sling_reference();
    se_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    int (*open_string)(void *) =
        (int (*)(void *))find_function("_Z14luaopen_stringP9lua_State");
    (void)open_string(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/powerup-uses/manager.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/shot-effects/redbird.lua");
    semihost(4, "AB_ANDROID_CLASSIC_SHOT_EFFECTS_V1 64\n");
    for (uint32_t sample = 0; sample < 64; ++sample) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, "m_powerupsUsed");
        damage_lua.get_field(lua, -10002, "PowerupManager");
        damage_lua.get_field(lua, 2, "hasPowerupBeenUsedInLevel");
        damage_lua.set_field(lua, 1, "hasPowerupBeenUsedInLevel");
        damage_lua.set_top(lua, 1);
        damage_lua.set_field(lua, -10002, "powerupManager");
        if (sample & 2) {
            damage_lua.get_field(lua, -10002, "PowerupManager");
            damage_lua.get_field(lua, 1, "markPowerupAsUsed");
            damage_lua.get_field(lua, -10002, "powerupManager");
            damage_lua.push_string(lua, "KingSling");
            if (damage_lua.call(lua, 2, 0, 0))
                fail("Original shot use-state setup failed\n");
            damage_lua.set_top(lua, 0);
        }
        if (!(sample & 1)) {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, "powerupManager");
        }
        damage_lua.push_boolean(lua, (sample & 1) != 0);
        damage_lua.set_field(lua, -10002, "g_powerups_enabled");
        damage_lua.push_boolean(lua, (sample & 16) != 0);
        damage_lua.set_field(lua, -10002, "g_red_bird_special_available");
        damage_lua.create_table(lua, 0, 0);
        damage_lua.push_string(lua, "fixture");
        damage_lua.set_field(lua, 1, "name");
        damage_lua.push_boolean(lua, (sample & 32) != 0);
        damage_lua.set_field(lua, 1, "hasCollided");
        uint32_t potion = (sample >> 2) & 3;
        if (potion == 0)
            damage_lua.push_nil(lua);
        else if (potion == 1)
            damage_lua.push_boolean(lua, 0);
        else if (potion == 2)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_string(lua, "");
        damage_lua.set_field(lua, 1, "usedPowerPotion");
        damage_lua.set_field(lua, -10002, "_ab_se_bird");
        uint32_t output[8] = {0};
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "attachParticleSpawnersToShotBird");
        se_push_value(lua, 1);
        damage_lua.get_field(lua, -10002, "_ab_se_bird");
        output[0] = damage_lua.call(lua, 2, 0, 0) == 0;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "redBirdShot");
        damage_lua.get_field(lua, -10002, "_ab_se_bird");
        output[1] = damage_lua.call(lua, 1, 0, 0) == 0;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_se_bird");
        se_encode(lua, 1, "shotWithKingSling", output + 2);
        se_encode(lua, 1, "shotWithPowerPotion", output + 4);
        se_encode(lua, 1, "hasCollided", output + 6);
        damage_lua.set_top(lua, 0);
        print_words(&sample, 1);
        print_words(output, 8);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB classic shot effects reference complete\n");
}

#endif
