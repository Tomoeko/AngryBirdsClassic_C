#ifndef AB_ORACLE_GAME_MODE_H
#define AB_ORACLE_GAME_MODE_H

#include "reference_game_mode.h"

static unsigned gm_profile, gm_step, gm_physics, gm_cursor, gm_has_cursor;
static unsigned gm_count, gm_created;
static uint32_t gm_rows[16][10];
static const void *gm_function_ids[32];

static unsigned gm_id(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "currentGameMode");
    unsigned id = 0;
    if (damage_lua.type(lua, -1) != 0) {
        const void *pointer = damage_lua.to_pointer(lua, -1);
        for (unsigned index = 0; index < gm_created; ++index)
            if (gm_function_ids[index] == pointer)
                id = index + 1;
        if (!id) {
            if (gm_created == 32)
                fail("Original game-mode identities exceed bound\n");
            gm_function_ids[gm_created++] = pointer;
            id = gm_created;
            damage_lua.get_field(lua, -10002, "_ab_mode_aliases");
            damage_lua.get_field(lua, -10002, "currentGameMode");
            void (*set_index)(void *, int, int) = (void (*)(
                void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
            set_index(lua, top + 2, (int)id);
        }
    }
    damage_lua.set_top(lua, top);
    return id;
}

static void gm_record(void *lua, unsigned kind) {
    if (gm_count == 16)
        fail("Original game-mode trace exceeds bound\n");
    uint32_t *row = gm_rows[gm_count++];
    for (unsigned index = 0; index < 10; ++index)
        row[index] = 0;
    row[0] = kind;
    int top = damage_lua.get_top(lua);
    unsigned count = kind == 1 ? 0u : (unsigned)top;
    if (count > 2)
        fail("Unexpected original game-mode service arguments\n");
    row[1] = count;
    for (unsigned index = 0; index < count; ++index) {
        unsigned type = (unsigned)damage_lua.type(lua, (int)index + 1);
        row[2 + index * 2] = type;
        if (type == 1)
            row[3 + index * 2] = (uint32_t)damage_lua.to_boolean(lua, (int)index + 1);
        else if (type == 3) {
            FloatBits value = {.number = damage_lua.to_number(lua, (int)index + 1)};
            row[3 + index * 2] = value.bits;
        } else if (type != 0)
            fail("Unexpected original game-mode argument type\n");
    }
    row[6] = gm_id(lua);
    row[7] = gm_physics;
    row[8] = gm_cursor;
    row[9] = gm_has_cursor;
}

static int gm_other_query(void *lua) {
    gm_record(lua, 1);
    unsigned kind = reference_game_mode(gm_profile).other_result;
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind < 3)
        damage_lua.push_boolean(lua, kind == 2);
    else
        damage_lua.push_number(lua, 0);
    return 1;
}

static int gm_set_physics(void *lua) {
    gm_physics = (unsigned)damage_lua.to_boolean(lua, 1);
    gm_record(lua, 2);
    return 0;
}

static int gm_motion_cursor(void *lua) {
    gm_cursor = (unsigned)damage_lua.to_boolean(lua, 1);
    gm_has_cursor = 1;
    gm_record(lua, 3);
    return 0;
}

static int gm_update(void *lua) {
    gm_record(lua, 4);
    return 0;
}

static int gm_draw(void *lua) {
    gm_record(lua, 5);
    return 0;
}

static int gm_other(void *lua) {
    gm_record(lua, 6);
    return 0;
}

static int gm_root(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_mode_root");
    return 1;
}

static int gm_child(void *lua) {
    if (!damage_named(lua, 2, "pausePage"))
        fail("Unexpected original game-mode child query\n");
    damage_lua.get_field(lua, -10002, "_ab_mode_pause");
    return 1;
}

static void gm_play(void *lua, unsigned museum) {
    damage_lua.get_field(lua, -10002, "modeSystem");
    damage_lua.get_field(lua, -10002, museum ? "_ab_museum" : "_ab_other_play");
    damage_lua.set_field(lua, 1, "playMode");
    damage_lua.set_top(lua, 0);
}

static void probe_game_mode(void) {
    initialize_damage_lua();
    semihost(4, "AB_ANDROID_GAME_MODE_V1 128 18\n");
    for (gm_profile = 0; gm_profile < 128; ++gm_profile) {
        ReferenceGameMode input = reference_game_mode(gm_profile);
        void *lua = damage_lua.new_state();
        if (!lua)
            fail("Cannot allocate original game-mode state\n");
        (void)damage_lua.open_base(lua);
        damage_lua.set_top(lua, 0);
        const char *files[] = {"mode-set.lua",          "mode-pause.lua",
                               "mode-ingame.lua",       "mode-is-level.lua",
                               "mode-is-museum.lua",    "mode-museum-level.lua",
                               "mode-pause-visible.lua"};
        const char *names[] = {"setGameMode",      "setGamePaused", "isIngame",
                               "_ab_is_level",     "_ab_is_museum", "_ab_museum_level",
                               "_ab_pause_visible"};
        for (unsigned index = 0; index < 7; ++index) {
            char path[128];
            const char *prefix = "/data/local/tmp/ab520/";
            unsigned offset = 0;
            while (prefix[offset]) {
                path[offset] = prefix[offset];
                ++offset;
            }
            for (unsigned byte = 0;; ++byte) {
                path[offset + byte] = files[index][byte];
                if (!files[index][byte])
                    break;
            }
            load_selected_function(lua, path, names[index]);
        }
        damage_function(lua, -10002, "updateGame", gm_update);
        damage_function(lua, -10002, "drawGame", gm_draw);
        damage_function(lua, -10002, "_ab_other_mode", gm_other);
        damage_function(lua, -10002, "setPhysicsEnabled", gm_set_physics);
        damage_function(lua, -10002, "setMotionRemoteCursorEnabled", gm_motion_cursor);
        if (input.accelerometer)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -10002, "g_isAccelerometerControl");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_boolean(lua, (int)input.show);
        damage_lua.set_field(lua, 1, "show");
        damage_lua.set_field(lua, -10002, "_ab_mode_pause");
        if (input.root) {
            damage_lua.create_table(lua, 0, 2);
            const char *roots[] = {"", "gameHud", "mainMenu", "other"};
            damage_lua.push_string(lua, roots[input.root]);
            damage_lua.set_field(lua, 1, "name");
            damage_function(lua, 1, "getChild", gm_child);
        } else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "_ab_mode_root");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 2, "getRoot", gm_root);
        damage_lua.set_field(lua, 1, "menuManager");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, "_ab_pause_visible");
        damage_lua.set_field(lua, 2, "isPauseMenuVisible");
        damage_lua.set_field(lua, 1, "subsystemsapi");
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, "_ab_museum_level");
        damage_lua.set_field(lua, 1, "isInLevel");
        damage_lua.set_field(lua, -10002, "_ab_museum");
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 1, "isInLevel", gm_other_query);
        damage_lua.set_field(lua, -10002, "_ab_other_play");
        damage_lua.create_table(lua, 0, 3);
        damage_lua.get_field(lua, -10002, "_ab_is_level");
        damage_lua.set_field(lua, 1, "isInLevel");
        damage_lua.get_field(lua, -10002, "_ab_is_museum");
        damage_lua.set_field(lua, 1, "isInMuseumMode");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, "_ab_museum");
        damage_lua.set_field(lua, 2, "MUSEUM");
        damage_lua.set_field(lua, 1, "modes");
        damage_lua.set_field(lua, -10002, "modeSystem");
        gm_play(lua, input.museum);
        gm_created = 0;
        const char *functions[] = {"updateGame", "drawGame", "_ab_other_mode"};
        for (unsigned index = 0; index < 3; ++index) {
            damage_lua.get_field(lua, -10002, functions[index]);
            gm_function_ids[gm_created++] = damage_lua.to_pointer(lua, 1);
            damage_lua.set_top(lua, 0);
        }
        damage_lua.get_field(lua, -10002, "_ab_other_mode");
        damage_lua.set_field(lua, -10002, "currentGameMode");
        damage_lua.create_table(lua, 16, 0);
        damage_lua.set_field(lua, -10002, "_ab_mode_aliases");
        gm_physics = 1;
        gm_cursor = gm_has_cursor = 0;
        unsigned failed = 0;
        for (gm_step = 0; gm_step < 18; ++gm_step) {
            gm_count = 0;
            unsigned operation = reference_game_mode_operation(gm_step);
            if (operation == REFERENCE_MODE_TOGGLE_PLAY)
                gm_play(lua, !input.museum);
            else if (operation == REFERENCE_MODE_FAIL_CURSOR) {
                damage_lua.push_number(lua, 0);
                damage_lua.set_field(lua, -10002, "g_isAccelerometerControl");
                damage_lua.push_nil(lua);
                damage_lua.set_field(lua, -10002, "setMotionRemoteCursorEnabled");
            } else if (operation != REFERENCE_MODE_SNAPSHOT) {
                if (operation == REFERENCE_MODE_CALL) {
                    damage_lua.get_field(lua, -10002, "currentGameMode");
                    damage_lua.push_number(lua, 0.125f);
                    damage_lua.push_number(lua, 7);
                } else if (operation == REFERENCE_MODE_SET_OTHER) {
                    damage_lua.get_field(lua, -10002, "setGameMode");
                    damage_lua.get_field(lua, -10002, "_ab_other_mode");
                } else {
                    damage_lua.get_field(lua, -10002, "setGamePaused");
                    if (operation == REFERENCE_MODE_PAUSE_ZERO)
                        damage_lua.push_number(lua, 0);
                    else
                        damage_lua.push_boolean(lua, operation == REFERENCE_MODE_PAUSE);
                }
                failed = damage_lua.call(lua, operation == REFERENCE_MODE_CALL ? 2 : 1,
                                         0, 0) != 0;
                damage_lua.set_top(lua, 0);
                if (failed && gm_step != 17)
                    fail("Unexpected early original game-mode error\n");
            }
            uint32_t state[] = {gm_profile,    gm_step,        failed,
                                gm_id(lua),    gm_physics,     gm_cursor,
                                gm_has_cursor, gm_created - 3, gm_count};
            print_words(state, 9);
            for (unsigned index = 0; index < gm_count; ++index)
                print_words(gm_rows[index], 10);
        }
        damage_lua.close_state(lua);
    }
    semihost(4, "AB game-mode reference complete\n");
}

#endif
