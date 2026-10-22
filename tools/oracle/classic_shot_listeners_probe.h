#ifndef AB_ORACLE_CLASSIC_SHOT_LISTENERS_H
#define AB_ORACLE_CLASSIC_SHOT_LISTENERS_H

#include "../../tests/classic_shot_listener_cases.h"

static void (*sl_push_value)(void *, int);
static uint32_t sl_sample, sl_saves, sl_power, sl_types, sl_texts, sl_analytics;
static uint32_t sl_notifications, sl_first, sl_seen, sl_level_tag, sl_score;
static uint32_t sl_order[2], sl_order_count;

static void sl_number(void *lua, int table, const char *name, float value) {
    damage_lua.push_number(lua, value);
    damage_lua.set_field(lua, table, name);
}

static void sl_boolean(void *lua, int table, const char *name, uint32_t flags,
                       uint32_t mask) {
    damage_lua.push_boolean(lua, (flags & mask) != 0);
    damage_lua.set_field(lua, table, name);
}

static void sl_record(uint32_t code) {
    if (sl_order_count >= 16)
        fail("Original shot listener boundary trace overflow\n");
    sl_order[sl_order_count / 8] |= code << (4 * (sl_order_count % 8));
    ++sl_order_count;
}

static int sl_save(void *lua) {
    if (damage_lua.get_top(lua) != 3 || !damage_named(lua, 1, "settings.lua") ||
        !damage_named(lua, 2, "settings") || !damage_lua.to_boolean(lua, 3))
        fail("Unexpected original achievement save request\n");
    ++sl_saves;
    sl_record(3);
    return 0;
}

static int sl_powerup(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 2, "birdquake"))
        fail("Unexpected original shot powerup query\n");
    ++sl_power;
    sl_record(4);
    damage_lua.push_boolean(lua, (sl_sample & 32) != 0);
    return 1;
}

static int sl_type(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "fixture-level-id"))
        fail("Unexpected original shot level-type query\n");
    ++sl_types;
    sl_record(5);
    damage_lua.push_string(lua, sl_sample % 5 == 0 ? "golden_egg" : "level");
    return 1;
}

static int sl_text(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 2, "fixture-level-id"))
        fail("Unexpected original shot level-text query\n");
    ++sl_texts;
    sl_record(6);
    if (sl_sample % 7 == 0)
        damage_lua.push_nil(lua);
    else
        damage_lua.push_string(lua, "fixture-level");
    return 1;
}

static int sl_log(void *lua) {
    if (damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 1, "Score generated prior to player input") ||
        damage_lua.type(lua, 2) != 5)
        fail("Unexpected original analytics request\n");
    ++sl_analytics;
    sl_record(7);
    damage_lua.get_field(lua, 2, "level");
    sl_level_tag = (uint32_t)damage_lua.type(lua, -1);
    if (sl_level_tag != 0 && !damage_named(lua, -1, "fixture-level"))
        fail("Original analytics level payload differs\n");
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "score");
    FloatBits score = {.number = damage_lua.to_number(lua, -1)};
    sl_score = score.bits;
    damage_lua.set_top(lua, 2);
    return 0;
}

static int sl_unlocked(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5)
        fail("Unexpected original achievement notification\n");
    damage_lua.get_field(lua, 2, "name");
    uint32_t id = damage_named(lua, -1, shot_listener_achievements[0]) ? 0u : 1u;
    if (id && !damage_named(lua, -1, shot_listener_achievements[1]))
        fail("Unexpected original achievement name\n");
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "first_time_unlocked");
    if (damage_lua.to_boolean(lua, -1))
        sl_first |= 1u << id;
    sl_seen |= 1u << id;
    ++sl_notifications;
    sl_record(id + 1);
    damage_lua.set_top(lua, 2);
    return 0;
}

static void sl_new(void *lua, const char *type, const char *name) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, type);
    damage_lua.get_field(lua, 1, "new");
    sl_push_value(lua, 1);
    if (damage_lua.call(lua, 1, 1, 0))
        fail("Original shot listener constructor failed\n");
    damage_lua.push_string(lua, "UNDEFINED");
    damage_lua.set_field(lua, 2, "name");
    damage_lua.set_field(lua, -10002, name);
    damage_lua.set_top(lua, 0);
}

static void sl_register(void *lua, const char *id, const char *name) {
    damage_lua.get_field(lua, -10002, "eventManager");
    damage_lua.get_field(lua, 1, "addEventListener");
    sl_push_value(lua, 1);
    damage_lua.push_string(lua, id);
    damage_lua.get_field(lua, -10002, name);
    if (damage_lua.call(lua, 3, 0, 0))
        fail("Original shot listener registration failed\n");
    damage_lua.set_top(lua, 0);
}

static void sl_prepare(void *lua, uint32_t sample) {
    sl_sample = sample;
    sl_saves = sl_power = sl_types = sl_texts = sl_analytics = 0;
    sl_notifications = sl_first = sl_seen = sl_level_tag = sl_score = 0;
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "settings");
    sl_boolean(lua, -10002, "cheatsEnabled", 0, 1);
    sl_boolean(lua, -10002, "releaseBuild", 1, 1);
    sl_boolean(lua, -10002, "iOS", 0, 1);
    sl_number(lua, -10002, "currentBirdIndex", sample & 8 ? 2.0f : 1.0f);
    sl_number(lua, -10002, "score", sample & 4 ? 123.0f : 0.0f);
    sl_boolean(lua, -10002, "g_powerups_enabled", sample, 16);
    damage_lua.push_string(lua, "fixture-level-id");
    damage_lua.set_field(lua, -10002, "levelName");
    sl_new(lua, "EventManager", "eventManager");
    sl_new(lua, "AchievementProcessor", "_ab_sl_achievement");
    sl_new(lua, "ClassicFlurry", "_ab_sl_flurry");
    damage_lua.get_field(lua, -10002, "_ab_sl_flurry");
    damage_function(lua, 1, "getLevelText", sl_text);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "settings");
    if (sample & 1)
        sl_number(lua, 1, "analyticsSessionId", 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.create_table(lua, 0, 0);
    sl_number(lua, 3, "installationId_fixture", 2);
    sl_number(lua, 3, "installationId_previous", 3);
    damage_lua.set_field(lua, 2, "device_fixture");
    damage_lua.set_field(lua, 1, "gameStarts");
    damage_lua.set_top(lua, 0);
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, "settingsWrapper");
        damage_lua.get_field(lua, 1, "getAchievementStatus");
        sl_push_value(lua, 1);
        damage_lua.push_string(lua, shot_listener_achievements[index]);
        if (damage_lua.call(lua, 2, 1, 0))
            fail("Original achievement status setup failed\n");
        sl_boolean(lua, 2, "unlocked", sample % 3 == 0 ? 1u : 0u, 1);
        damage_lua.set_top(lua, 0);
    }
    if (sample % 11) {
        damage_lua.create_table(lua, 0, 0);
        damage_function(lua, 1, "logEvent", sl_log);
        damage_lua.set_field(lua, -10002, "RovioAnalytics");
    } else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "RovioAnalytics");
    }
    damage_lua.create_table(lua, 0, 0);
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, 1, "name");
    damage_function(lua, 1, "eventTriggered", sl_unlocked);
    damage_lua.set_field(lua, -10002, "_ab_sl_observer");
    sl_register(lua, "EID_BIRD_SHOT", "_ab_sl_achievement");
    sl_register(lua, "EID_BIRD_SHOT", "_ab_sl_flurry");
    sl_register(lua, "EID_ACHIEVEMENT_UNLOCKED", "_ab_sl_observer");
}

static void sl_snapshot(void *lua, uint32_t output[SHOT_LISTENER_WORDS]) {
    output[1] = sl_saves;
    output[2] = sl_power;
    output[3] = sl_types;
    output[4] = sl_texts;
    output[5] = sl_analytics;
    output[6] = sl_notifications;
    output[7] = sl_first;
    output[8] = sl_seen;
    output[9] = sl_level_tag;
    output[10] = sl_score;
    damage_lua.get_field(lua, -10002, "settings");
    damage_lua.get_field(lua, 1, "analyticsSessionId");
    output[11] = (uint32_t)damage_lua.type(lua, -1);
    FloatBits session = {.number = damage_lua.to_number(lua, -1)};
    output[12] = session.bits;
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "achievementStatus");
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, 2, shot_listener_achievements[index]);
        damage_lua.get_field(lua, 3, "unlocked");
        output[13 + index] = damage_lua.to_boolean(lua, -1) != 0;
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    output[15] = sl_order[0];
    output[16] = sl_order[1];
    damage_lua.get_field(lua, -10002, "_ab_sl_achievement");
    damage_lua.get_field(lua, 1, "birdSlingerLimit");
    FloatBits limit = {.number = damage_lua.to_number(lua, -1)};
    output[17] = limit.bits;
    damage_lua.set_top(lua, 0);
}

static void probe_classic_shot_listeners(void) {
    initialize_damage_lua();
    sl_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original shot listener state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_function(lua, -10002, "print", damage_require);
    damage_function(lua, -10002, "printError", damage_require);
    damage_function(lua, -10002, "saveLuaFileWrapper", sl_save);
    damage_function(lua, -10002, "getLevelType", sl_type);
    damage_lua.get_field(lua, -10002, "_G");
    sl_push_value(lua, 1);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "lua");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "classicEvents");
    damage_lua.create_table(lua, 0, 0);
    damage_function(lua, 1, "hasPowerupBeenUsedInLevel", sl_powerup);
    damage_lua.set_field(lua, -10002, "powerupManager");
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueDeviceId");
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueInstallationId");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "settings");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/utils.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/manager.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/shot-listeners/events.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/shot-listeners/subsystems.lua");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.get_field(lua, -10002, "SubSystem");
    damage_lua.set_field(lua, 1, "SubSystem");
    damage_lua.set_field(lua, -10002, "subsystems");
    load_damage_chunk(lua, "/data/local/tmp/ab520/statistics/settings.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/shot-listeners/achievements.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/shot-listeners/flurry-base.lua");
    damage_lua.get_field(lua, -10002, "subsystems");
    damage_lua.get_field(lua, -10002, "Flurry");
    damage_lua.set_field(lua, 1, "Flurry");
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/shot-listeners/flurry-classic.lua");
    semihost(4, "AB_ANDROID_CLASSIC_SHOT_LISTENERS_V1 64 2\n");
    for (uint32_t sample = 0; sample < SHOT_LISTENER_CASES; ++sample) {
        sl_prepare(lua, sample);
        print_words(&sample, 1);
        for (uint32_t pass = 0; pass < SHOT_LISTENER_PASSES; ++pass) {
            sl_order[0] = sl_order[1] = sl_order_count = 0;
            damage_lua.get_field(lua, -10002, "eventManager");
            damage_lua.get_field(lua, 1, "notify");
            sl_push_value(lua, 1);
            damage_lua.create_table(lua, 0, 0);
            damage_lua.push_string(lua, "EID_BIRD_SHOT");
            damage_lua.set_field(lua, 4, "id");
            sl_number(lua, 4, "birdsShooted", sample & 1 ? 5000.0f : 4999.0f);
            sl_number(lua, 4, "backwardsBirdCount", sample & 2 ? 10.0f : 9.0f);
            uint32_t output[SHOT_LISTENER_WORDS] = {0};
            output[0] = damage_lua.call(lua, 2, 0, 0) == 0;
            damage_lua.set_top(lua, 0);
            sl_snapshot(lua, output);
            print_words(output, SHOT_LISTENER_WORDS);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB classic shot listeners reference complete\n");
}

#endif
