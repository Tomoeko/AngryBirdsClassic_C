#ifndef AB_ORACLE_GAME_UPDATE_TAIL_H
#define AB_ORACLE_GAME_UPDATE_TAIL_H

static uint32_t tail_input[22], tail_events[48][14], tail_event_count;
static bool tail_active;
static int (*tail_next)(void *, int);
static const char *const tail_names[] = {"first", "second", "third"};

static bool tail_flag(uint32_t bit) {
    return (tail_input[8] & bit) != 0;
}

static uint32_t tail_global_truth(void *lua, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    uint32_t result = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, top);
    return result;
}

static bool tail_optional(void *lua, const char *name, uint32_t *word) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    int kind = damage_lua.type(lua, -1);
    *word = 0;
    if (kind == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        *word = value.bits;
    } else if (kind != 0) {
        fail("Unexpected original game tail optional number\n");
    }
    damage_lua.set_top(lua, top);
    return kind == 3;
}

static uint32_t tail_state_flags(void *lua) {
    return tail_global_truth(lua, "birdFired") |
           (tail_global_truth(lua, "g_red_bird_special_available") << 1) |
           (tail_global_truth(lua, "mightyTimerUpdate") << 2) |
           (tail_global_truth(lua, "skipInput") << 3) |
           (tail_global_truth(lua, "g_skip_trigger_specialty") << 4);
}

static uint32_t tail_object(void *lua, int argument) {
    if (damage_lua.type(lua, argument) == 5) {
        FloatBits value = {.bits = mouse_field(lua, argument, "id")};
        uint32_t id = (uint32_t)value.number;
        if (id < 1 || id > 3 || value.number != (float)id) {
            fail("Invalid original game tail object identity\n");
        }
        return id;
    }
    for (size_t index = 0; index < 3; ++index) {
        if (damage_named(lua, argument, tail_names[index])) {
            return (uint32_t)index + 1;
        }
    }
    fail("Unknown original game tail object argument\n");
}

static void tail_record(void *lua, uint32_t kind, uint32_t object, int argc,
                        int seconds, int time) {
    if (!tail_active || tail_event_count >= 48 || damage_lua.get_top(lua) != argc) {
        fail("Unexpected original game tail call/arity\n");
    }
    uint32_t *event = tail_events[tail_event_count++];
    for (size_t index = 0; index < 14; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    event[1] = object;
    event[2] = (uint32_t)argc;
    if (seconds) {
        if (damage_lua.type(lua, seconds) != 3) {
            fail("Missing original game tail elapsed argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, seconds)};
        event[3] = value.bits;
    }
    if (time) {
        if (damage_lua.type(lua, time) != 3) {
            fail("Missing original game tail timestamp argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, time)};
        event[4] = value.bits;
    }
    mouse_global_vector(lua, "cursor", event + 5);
    mouse_global_vector(lua, "oldCursor", event + 7);
    event[9] = tail_state_flags(lua);
    event[10] = tail_optional(lua, "eagleDarkness", event + 11);
    event[12] = tail_optional(lua, "timeSinceLevelStart", event + 13);
}

static void tail_move_cursor(void *lua, float dx, float dy) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "cursor");
    int table = top + 1;
    FloatBits x = {.bits = mouse_field(lua, table, "x")};
    FloatBits y = {.bits = mouse_field(lua, table, "y")};
    x.number += dx;
    y.number += dy;
    damage_number(lua, table, "x", x.bits);
    damage_number(lua, table, "y", y.bits);
    damage_lua.set_top(lua, top);
}

static int tail_script(void *lua);

static void tail_scripts(void *lua, uint32_t mask) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, top + 1, "world");
    int world = top + 2;
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, world, tail_names[index]);
        if (mask & (1u << index)) {
            damage_lua.create_table(lua, 0, 2);
            FloatBits id = {.number = (float)index + 1};
            damage_number(lua, top + 4, "id", id.bits);
            damage_function(lua, top + 4, "update", tail_script);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, top + 3, "gameObjectScript");
        damage_lua.set_top(lua, world);
    }
    damage_lua.set_top(lua, top);
}

static int tail_mighty(void *lua) {
    tail_record(lua, 8, 0, 1, 1, 0);
    return 0;
}

static void tail_mighty_presence(void *lua, bool present) {
    if (present) {
        damage_function(lua, -10002, "mightyTimerUpdate", tail_mighty);
    } else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "mightyTimerUpdate");
    }
}

static int tail_begin_phase(void *lua) {
    tail_active = true;
    tail_record(lua, 1, 0, 2, 2, 0);
    if (tail_flag(128)) {
        mouse_boolean(lua, -10002, "birdFired", !tail_flag(1));
        mouse_boolean(lua, -10002, "g_red_bird_special_available", !tail_flag(8));
        tail_mighty_presence(lua, !tail_flag(4));
        uint32_t raw;
        if (tail_optional(lua, "eagleDarkness", &raw)) {
            FloatBits value = {.bits = raw};
            value.number += 0.125f;
            damage_number(lua, -10002, "eagleDarkness", value.bits);
        }
    }
    return 0;
}

static int tail_wobble(void *lua) {
    tail_record(lua, 2, 0, 2, 2, 0);
    return 0;
}

static int tail_animation(void *lua) {
    tail_record(lua, 3, tail_object(lua, 1), 3, 3, 0);
    return 0;
}

static int tail_rocket(void *lua) {
    tail_record(lua, 4, tail_object(lua, 1), 3, 3, 0);
    return 0;
}

static int tail_bird(void *lua) {
    uint32_t object = tail_object(lua, 1);
    tail_record(lua, 5, object, 3, 3, 0);
    damage_lua.push_boolean(lua, (tail_input[9] & (1u << (object - 1))) != 0);
    return 1;
}

static int tail_boundaries(void *lua) {
    uint32_t object = tail_object(lua, 1);
    tail_record(lua, 6, object, 3, 3, 0);
    damage_lua.push_boolean(lua, (tail_input[10] & (1u << (object - 1))) != 0);
    return 1;
}

static int tail_object_effect(void *lua) {
    tail_record(lua, 7, tail_object(lua, 2), 4, 4, 0);
    return 0;
}

static int tail_camera(void *lua) {
    tail_record(lua, 9, 0, 2, 2, 0);
    return 0;
}

static int tail_boomerang(void *lua) {
    tail_record(lua, 10, 0, 1, 1, 0);
    return 0;
}

static int tail_characters(void *lua) {
    tail_record(lua, 11, 0, 1, 1, 0);
    return 0;
}

static int tail_floating(void *lua) {
    tail_record(lua, 12, 0, 1, 1, 0);
    return 0;
}

static int tail_score(void *lua) {
    tail_record(lua, 13, 0, 1, 1, 0);
    if (tail_flag(64)) {
        tail_move_cursor(lua, 0.5f, -0.25f);
    }
    if (tail_flag(128)) {
        tail_scripts(lua, tail_input[11] ^ 7);
    }
    return 0;
}

static int tail_feathers(void *lua) {
    tail_record(lua, 14, 0, 1, 1, 0);
    return 0;
}

static int tail_physics_enabled(void *lua) {
    tail_record(lua, 15, 0, 0, 0, 0);
    damage_lua.push_boolean(lua, tail_flag(16));
    return 1;
}

static int tail_level_effects(void *lua) {
    tail_record(lua, 16, 0, 2, 1, 2);
    return 0;
}

static int tail_blocks(void *lua) {
    tail_record(lua, 17, 0, 2, 1, 2);
    return 0;
}

static int tail_script(void *lua) {
    tail_record(lua, 18, tail_object(lua, 1), 3, 2, 3);
    return 0;
}

static int tail_gfx(void *lua) {
    tail_record(lua, 19, 0, 1, 1, 0);
    if (tail_flag(64)) {
        tail_move_cursor(lua, 0, 1);
    }
    return 0;
}

static int tail_draw(void *lua) {
    tail_record(lua, 20, 0, 0, 0, 0);
    if (tail_flag(128)) {
        damage_number(lua, -10002, "timeSinceLevelStart", UINT32_C(0x3fa00000));
    }
    return 0;
}

static void tail_globals(void *lua) {
    const char *names[] = {"updateAnimation",
                           "updateRocket",
                           "updateBird",
                           "updateObjectOutsideBoundaries",
                           "updateBoomerangBirds",
                           "updateCharacterAnimations",
                           "updateFloatingScores",
                           "updateScore",
                           "updateSpecialFeathers",
                           "isPhysicsEnabled",
                           "updateLevelEffects",
                           "drawGame"};
    int (*functions[])(void *) = {
        tail_animation, tail_rocket,          tail_bird,          tail_boundaries,
        tail_boomerang, tail_characters,      tail_floating,      tail_score,
        tail_feathers,  tail_physics_enabled, tail_level_effects, tail_draw};
    for (size_t index = 0; index < sizeof(names) / sizeof(*names); ++index) {
        damage_function(lua, -10002, names[index], functions[index]);
    }
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_function(lua, 1, "updateCamera", tail_camera);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_mouse_sling");
    damage_function(lua, 1, "updateRubberBandWobble", tail_wobble);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "updateObject", tail_object_effect);
    damage_lua.set_field(lua, -10002, "g_levelEffect");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "updateGFXEffects", tail_gfx);
    damage_lua.set_field(lua, -10002, "CameraNative");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "update", tail_blocks);
    damage_lua.set_field(lua, 1, "BlockScriptManager");
    damage_lua.set_field(lua, -10002, "blocks");
    const char *empty[] = {"flyingGrenades", "g_jointsToDestroy"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, empty[index]);
    }
    damage_lua.get_field(lua, -10002, "_ab_mouse_update");
    damage_lua.create_table(lua, 0, 0);
    const char *(*setup)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    if (!setup(lua, 1, 2)) {
        fail("Cannot seed original empty custom-update table\n");
    }
    damage_lua.set_top(lua, 0);
}

static void tail_prepare(void *lua) {
    for (size_t index = 0; index < 36; ++index) {
        mouse_input[index] = 0;
    }
    mouse_input[0] = tail_input[0];
    mouse_input[1] = tail_input[4];
    mouse_input[2] = tail_input[5];
    mouse_input[3] = tail_input[6];
    mouse_input[4] = tail_input[7];
    mouse_input[14] = UINT32_C(0x3f800000);
    mouse_input[15] = UINT32_C(0x43f00000);
    mouse_input[17] = 8;
    mouse_input[18] = UINT32_C(0x3f800000);
    mouse_camera[26] = mouse_camera[27] = 0;
    mouse_camera[28] = mouse_camera[29] = mouse_input[14];
    mouse_prepare(lua);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "joints");
    damage_lua.get_field(lua, 1, "world");
    for (size_t index = 0; index < 3; ++index) {
        if (index) {
            damage_lua.create_table(lua, 0, 1);
        } else {
            damage_lua.get_field(lua, 2, "first");
        }
        FloatBits id = {.number = (float)index + 1};
        damage_number(lua, 3, "id", id.bits);
        damage_lua.set_field(lua, 2, tail_names[index]);
    }
    damage_lua.set_top(lua, 0);
    tail_scripts(lua, tail_input[11]);
    mouse_boolean(lua, -10002, "birdFired", tail_flag(1));
    mouse_boolean(lua, -10002, "g_red_bird_special_available", tail_flag(8));
    mouse_boolean(lua, -10002, "g_skip_trigger_specialty", true);
    tail_mighty_presence(lua, tail_flag(4));
    const char *optional[] = {"eagleDarkness", "timeSinceLevelStart"};
    const uint32_t present[] = {2, 32};
    for (size_t index = 0; index < 2; ++index) {
        if (tail_flag(present[index])) {
            damage_number(lua, -10002, optional[index], tail_input[2 + index]);
        } else {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, optional[index]);
        }
    }
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, 1, "world");
    damage_lua.push_nil(lua);
    size_t count = 0;
    while (tail_next(lua, 2)) {
        if (count == 3) {
            fail("Unexpected original world traversal count\n");
        }
        tail_input[12 + count++] = tail_object(lua, 4);
        damage_lua.set_top(lua, 3);
    }
    if (count != 3) {
        fail("Missing original world traversal entries\n");
    }
    damage_lua.set_top(lua, 0);
}

static uint32_t tail_script_mask(void *lua) {
    uint32_t mask = 0;
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, 1, "world");
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, 2, tail_names[index]);
        damage_lua.get_field(lua, 3, "gameObjectScript");
        int kind = damage_lua.type(lua, 4);
        if (kind == 5) {
            mask |= 1u << index;
        } else if (kind != 0) {
            fail("Unexpected original object-script field\n");
        }
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    return mask;
}

#if defined(AB_ORACLE_GAME_UPDATE_HOOKS_ONLY)
static int tail_hook(void *lua) {
    uint32_t id = tail_object(lua, 1);
    tail_record(lua, 22, id, 2, 2, 0);
    if (tail_input[16] & (1u << (id - 1))) {
        tail_move_cursor(lua, 0.25f, -0.5f);
        mouse_boolean(lua, -10002, "birdFired", !tail_global_truth(lua, "birdFired"));
        uint32_t bits;
        if (tail_optional(lua, "eagleDarkness", &bits)) {
            FloatBits darkness = {.bits = bits};
            darkness.number += 0.125f;
            damage_number(lua, -10002, "eagleDarkness", darkness.bits);
        }
    }
    return 0;
}

static void tail_prepare_hooks(void *lua) {
    const char *names[] = {"first", "second", "third", "missing"};
    damage_lua.get_field(lua, -10002, "_ab_mouse_update");
    damage_lua.create_table(lua, 0, 0);
    for (size_t index = 0; index < 4; ++index) {
        tail_input[18 + index] = 0;
        if (tail_input[15] & (1u << index)) {
            damage_function(lua, 2, names[index], tail_hook);
        }
    }
    damage_lua.push_nil(lua);
    size_t count = 0;
    while (tail_next(lua, 2)) {
        uint32_t id = 0;
        for (size_t index = 0; index < 4; ++index) {
            if (damage_named(lua, 3, names[index])) {
                id = (uint32_t)index + 1;
            }
        }
        if (!id || count == 4) {
            fail("Unexpected native hook traversal key\n");
        }
        tail_input[18 + count++] = id;
        damage_lua.set_top(lua, 3);
    }
    tail_input[17] = (uint32_t)count;
    const char *(*setup)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    if (!setup(lua, 1, 2)) {
        fail("Cannot seed original registered object hooks\n");
    }
    damage_lua.set_top(lua, 0);
}

static uint32_t tail_hook_mask(void *lua) {
    const char *names[] = {"first", "second", "third", "missing"};
    damage_lua.get_field(lua, -10002, "_ab_mouse_update");
    const char *(*upvalue)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_getupvalueP9lua_Stateii");
    if (!upvalue(lua, 1, 2)) {
        fail("Cannot read original registered object hooks\n");
    }
    uint32_t mask = 0;
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, 2, names[index]);
        if (damage_lua.type(lua, 3) == 6) {
            mask |= 1u << index;
        } else if (damage_lua.type(lua, 3) != 0) {
            fail("Unexpected original registered hook value\n");
        }
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    return mask;
}
#endif

#if defined(AB_ORACLE_GAME_UPDATE_JOINTS_ONLY)
#include "game_update_joints_probe.h"
#endif

static void probe_game_update_tail(void) {
    void *lua = mouse_open_reference();
    tail_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    tail_globals(lua);
    uint32_t random = UINT32_C(0x5207a117);
#if defined(AB_ORACLE_GAME_UPDATE_JOINTS_ONLY)
    semihost(4, "AB_ANDROID_GAME_UPDATE_JOINTS_V1 4096\n");
#elif defined(AB_ORACLE_GAME_UPDATE_HOOKS_ONLY)
    semihost(4, "AB_ANDROID_GAME_UPDATE_HOOKS_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_GAME_UPDATE_TAIL_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 8; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 16};
            tail_input[field] = value.bits;
        }
        FloatBits seconds = {.number = (float)(random_word(&random) % 513) / 1024};
        tail_input[0] = seconds.bits;
        if (item < 32) {
            tail_input[0] = item & 1 ? UINT32_C(0x80000000) : 0;
            tail_input[2] = item & 2 ? UINT32_C(0x80000000) : 0;
            tail_input[3] = item & 4 ? UINT32_C(0x80000000) : 0;
        }
        tail_input[8] = (uint32_t)item & 255;
        tail_input[9] = (random_word(&random) >> 16) & 7;
        tail_input[10] = (random_word(&random) >> 16) & 7;
        tail_input[11] = (random_word(&random) >> 16) & 7;
        tail_prepare(lua);
#if defined(AB_ORACLE_GAME_UPDATE_JOINTS_ONLY)
        tail_prepare_joints(lua, (uint32_t)item);
#endif
#if defined(AB_ORACLE_GAME_UPDATE_HOOKS_ONLY)
        tail_input[15] = (uint32_t)(item & 15);
        tail_input[16] = (uint32_t)((item / 16) & 7);
        tail_prepare_hooks(lua);
#endif
        tail_active = false;
        mouse_phase = false;
        tail_event_count = 0;
        damage_lua.get_field(lua, -10002, "_ab_mouse_update");
        FloatBits time = {.bits = tail_input[1]};
        seconds.bits = tail_input[0];
        damage_lua.push_number(lua, seconds.number);
        damage_lua.push_number(lua, time.number);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original game update tail failed\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t output[12] = {0};
        mouse_global_vector(lua, "cursor", output);
        mouse_global_vector(lua, "oldCursor", output + 2);
        output[4] = tail_optional(lua, "eagleDarkness", output + 5);
        output[6] = tail_optional(lua, "timeSinceLevelStart", output + 7);
        output[8] = tail_state_flags(lua);
        output[9] = tail_script_mask(lua);
        output[10] = tail_event_count;
#if defined(AB_ORACLE_GAME_UPDATE_JOINTS_ONLY)
        output[11] = tail_joint_queue_count(lua);
        print_words(tail_input, 22);
#elif defined(AB_ORACLE_GAME_UPDATE_HOOKS_ONLY)
        output[11] = tail_hook_mask(lua);
        print_words(tail_input, 22);
#else
        print_words(tail_input, 15);
#endif
        print_words(output, 12);
        for (size_t index = 0; index < tail_event_count; ++index) {
            print_words(tail_events[index], 14);
        }
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_GAME_UPDATE_JOINTS_ONLY)
    semihost(4, "AB game update joints reference complete\n");
#elif defined(AB_ORACLE_GAME_UPDATE_HOOKS_ONLY)
    semihost(4, "AB game update hooks reference complete\n");
#else
    semihost(4, "AB game update tail reference complete\n");
#endif
}

#endif
