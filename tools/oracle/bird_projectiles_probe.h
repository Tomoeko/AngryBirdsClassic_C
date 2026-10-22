#ifndef AB_ORACLE_BIRD_PROJECTILES_H
#define AB_ORACLE_BIRD_PROJECTILES_H

#include "bird_notify_probe.h"
#include "bird_collision_frame.h"
#include "../../tests/bird_projectiles_cases.h"

static uint32_t projectiles_input[25], projectiles_events[2][148], projectiles_count;
static uint32_t projectiles_length, projectiles_return[3], projectiles_returns;
static void (*projectiles_raw_set_i)(void *, int, int);
static void (*projectiles_raw_get_i)(void *, int, int);
static int (*projectiles_set_hook)(void *, void (*)(void *, void *), int, int);
static const char *const projectiles_objects[] = {
    "first",        "second",       "_ab_grenade2", "_ab_grenade3", "_ab_grenade4",
    "_ab_grenade5", "_ab_grenade6", "_ab_grenade7", "_ab_grenade8", "_ab_grenade9"};
static const char *const projectiles_names[] = {"first", "second", "other"};
static const char *const projectiles_values[] = {"truthy", "BOOMERANG", "OTHER"};

static uint32_t projectiles_name_id(void *lua, int index) {
    for (uint32_t id = 0; id < 3; ++id) {
        if (damage_named(lua, index, projectiles_names[id])) {
            return id + 1u;
        }
    }
    fail("Unexpected original projectile object name\n");
    return 0;
}

static void projectiles_scalar(void *lua, uint32_t kind) {
    if (kind == 5) {
        damage_lua.get_field(lua, -10002, "_ab_projectile_value");
    } else {
        bird_notify_scalar(lua, kind);
    }
}

static void projectiles_specialty(void *lua, uint32_t kind) {
    if (kind == 4 || kind == 5) {
        damage_lua.push_string(lua, kind == 4 ? "BOOMERANG" : "OTHER");
    } else {
        projectiles_scalar(lua, kind == 6 ? 5u : kind);
    }
}

static void projectiles_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 0; id < 3; ++id) {
            if (damage_named(lua, -1, projectiles_values[id])) {
                output[1] = id + 1u;
                return;
            }
        }
        fail("Unexpected original projectile field string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] != 201) {
            fail("Unexpected original projectile field table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original projectile field type\n");
    }
}

static int projectiles_list(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "flyingGrenades");
    return top + 2;
}

static void projectiles_snapshot(void *lua, uint32_t output[145]) {
    int top = damage_lua.get_top(lua);
    static const char *const fields[] = {"boomerangActive", "collision", "explode",
                                         "controllable",    "shot",      "sensor"};
    for (size_t id = 0; id < 10; ++id) {
        damage_lua.get_field(lua, -10002, projectiles_objects[id]);
        damage_lua.get_field(lua, top + 1, "name");
        output[id * 13u] = projectiles_name_id(lua, -1);
        damage_lua.set_top(lua, top + 1);
        for (size_t field = 0; field < 6; ++field) {
            damage_lua.get_field(lua, top + 1, fields[field]);
            projectiles_value(lua, output + id * 13u + 1u + field * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002,
                             id ? "_ab_projectile_def2" : "_ab_projectile_def1");
        damage_lua.get_field(lua, top + 1, "specialty");
        projectiles_value(lua, output + 130 + id * 2u);
        damage_lua.set_top(lua, top);
    }
    int list = projectiles_list(lua);
    output[134] = damage_lua.type(lua, list) == 5;
    output[135] = output[134] ? projectiles_length : 0;
    for (uint32_t slot = 0; slot < 9; ++slot) {
        output[136 + slot] = 0;
        if (output[134] && slot < projectiles_length) {
            projectiles_raw_get_i(lua, list, (int)slot + 1);
            damage_lua.get_field(lua, list + 1, "_id");
            output[136 + slot] = (uint32_t)damage_lua.to_number(lua, -1);
            damage_lua.set_top(lua, list);
        }
    }
    damage_lua.set_top(lua, top);
}

static void projectiles_field(void *lua, const char *object, const char *field,
                              uint32_t kind) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    projectiles_scalar(lua, kind);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static int projectiles_definition(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc == 44 || pc == 48 || pc == 378 || pc == 444) {
        damage_lua.create_table(lua, 0, 0);
        return 1;
    }
    if ((pc != 747 && pc != 762) || damage_lua.get_top(lua) != 1 ||
        projectiles_count >= 2) {
        fail("Unexpected original bird projectile definition boundary\n");
    }
    uint32_t id = projectiles_name_id(lua, 1);
    if (id != (pc == 747 ? 1u : 2u)) {
        fail("Original projectile definition name mismatch\n");
    }
    uint32_t *event = projectiles_events[projectiles_count++];
    event[0] = pc;
    event[1] = id;
    event[2] = 1;
    projectiles_snapshot(lua, event + 3);
    uint32_t mutation = projectiles_input[11];
    if (id == 1) {
        if (mutation & 1u) {
            projectiles_field(lua, "second", "sensor", 2);
        }
        if (mutation & 2u) {
            projectiles_field(lua, "second", "controllable", 2);
        }
        if (mutation & 4u) {
            projectiles_field(lua, "first", "shot", 1);
        }
        if (mutation & 8u) {
            damage_lua.get_field(lua, -10002, "_ab_projectile_def2");
            damage_lua.push_string(lua, "BOOMERANG");
            damage_lua.set_field(lua, 2, "specialty");
            damage_lua.set_top(lua, 1);
        }
        if ((mutation & 32u) && projectiles_length) {
            int list = projectiles_list(lua);
            if (damage_lua.type(lua, list) == 5) {
                projectiles_raw_get_i(lua, list, (int)projectiles_length);
                damage_lua.get_field(lua, list + 1, "_id");
                uint32_t object = (uint32_t)damage_lua.to_number(lua, -1);
                damage_lua.set_top(lua, list + 1);
                if (object >= 3) {
                    damage_lua.push_string(lua, "first");
                    damage_lua.set_field(lua, list + 1, "name");
                }
            }
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 64u) {
            damage_lua.get_field(lua, -10002, "gamelua");
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, 2, "flyingGrenades");
            damage_lua.set_top(lua, 1);
        }
    } else {
        if ((mutation & 16u) && projectiles_length < 9) {
            int list = projectiles_list(lua);
            if (damage_lua.type(lua, list) == 5) {
                damage_lua.get_field(lua, -10002, "first");
                projectiles_raw_set_i(lua, list, (int)++projectiles_length);
            }
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 128u) {
            projectiles_field(lua, "first", "controllable", 1);
        }
    }
    if (projectiles_input[8] & (1u << (id - 1u))) {
        damage_lua.get_field(lua, -10002,
                             id == 1 ? "_ab_projectile_def1" : "_ab_projectile_def2");
    } else {
        damage_lua.push_nil(lua);
    }
    return 1;
}

static int projectiles_damage_sprite(void *lua) {
    if (bird_collision_caller_pc(lua) != 959 || damage_lua.get_top(lua) != 2) {
        fail("Unexpected original projectile suffix damage-sprite boundary\n");
    }
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 2.0f || damage_lua.type(lua, 2) != 5) {
        fail("Unexpected original projectile suffix damage-sprite arguments\n");
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "old2");
    damage_lua.set_field(lua, 4, "sprite");
    return 1;
}

/* The original debug API installs a return hook in this isolated test state.
 * Inside it, only read ARM32 words: getlocal ELF 0x83ba08 confirms ci.base+0;
 * getinfo confirms current savedpc is lua_State+0x18. Filter non-bird frames,
 * then check the unchanged 974-instruction prototype before reading R0/R1.
 * No hook call allocates, invokes Lua, or changes a register/instruction. */
static void projectiles_return_hook(void *lua, void *debug) {
    const uint32_t *state = lua;
    const uint32_t *ci = (const uint32_t *)(uintptr_t)state[5];
    const uint32_t *function = (const uint32_t *)(uintptr_t)ci[1];
    if (function[1] != 6 || !function[0]) {
        return;
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] || !closure[4]) {
        return;
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 974) {
        return;
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t saved = state[6], begin = prototype[3];
    if (*(const uint32_t *)debug != 1 || ((const uint8_t *)closure)[7] != 1 ||
        code[0] != UINT32_C(0x00000205) || code[973] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 974u * 4u || (saved - begin) % 4u != 0 ||
        projectiles_returns++) {
        fail("Unexpected original bird return-hook frame\n");
    }
    projectiles_return[0] = (uint32_t)((saved - begin) / 4u - 1u);
    const uint32_t *base = (const uint32_t *)(uintptr_t)ci[0];
    projectiles_return[1] = base[1];
    projectiles_return[2] = base[3];
    if ((projectiles_return[0] != 819 && projectiles_return[0] != 825 &&
         projectiles_return[0] != 973) ||
        (base[1] != 0 && base[1] != 4) || base[3] != 4) {
        fail("Unexpected original bird return instruction/local-name tags\n");
    }
}

static void projectiles_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 201);
    damage_lua.set_field(lua, 1, "_id");
    damage_lua.set_field(lua, -10002, "_ab_projectile_value");
    for (uint32_t id = 0; id < 10; ++id) {
        if (id < 2) {
            damage_lua.get_field(lua, -10002, projectiles_objects[id]);
        } else {
            damage_lua.create_table(lua, 0, 8);
            damage_lua.push_number(lua, (float)id + 1.0f);
            damage_lua.set_field(lua, 1, "_id");
            damage_lua.push_string(
                lua,
                projectiles_names[bird_projectile_name(projectiles_input[0], id) - 1u]);
            damage_lua.set_field(lua, 1, "name");
        }
        projectiles_scalar(lua, (projectiles_input[16] + id) % 6u);
        damage_lua.set_field(lua, 1, "explode");
        if (id < 2) {
            projectiles_scalar(lua, projectiles_input[2 + id]);
            damage_lua.set_field(lua, 1, "controllable");
            projectiles_scalar(lua, projectiles_input[4 + id]);
            damage_lua.set_field(lua, 1, "shot");
            projectiles_scalar(lua, projectiles_input[12 + id]);
            damage_lua.set_field(lua, 1, "boomerangActive");
            projectiles_scalar(lua, projectiles_input[14 + id]);
            damage_lua.set_field(lua, 1, "collision");
            damage_lua.push_string(lua, id ? "old2" : "old1");
            damage_lua.set_field(lua, 1, "damageSprite");
        }
        damage_lua.set_field(lua, -10002, projectiles_objects[id]);
    }
    projectiles_field(lua, "second", "sensor", projectiles_input[1]);
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.create_table(lua, 0, 1);
        projectiles_specialty(lua, projectiles_input[6 + id]);
        damage_lua.set_field(lua, 1, "specialty");
        damage_lua.set_field(lua, -10002,
                             id ? "_ab_projectile_def2" : "_ab_projectile_def1");
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", projectiles_definition);
    damage_function(lua, 1, "getDamageSprite", projectiles_damage_sprite);
    static const char *const empty_tables[] = {"g_birdHats", "achievementProcessor",
                                               "birds"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, empty_tables[field]);
    }
    damage_number(lua, 1, "collisionParticleForceThreshold", UINT32_C(0x7f7fffff));
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "blockTable");
    if (projectiles_input[9]) {
        damage_lua.create_table(lua, 9, 0);
        for (uint32_t slot = 0; slot < projectiles_length; ++slot) {
            damage_lua.get_field(lua, -10002,
                                 projectiles_objects[projectiles_input[17 + slot]]);
            projectiles_raw_set_i(lua, 2, (int)slot + 1);
        }
        damage_lua.set_field(lua, 1, "flyingGrenades");
    }
    damage_lua.set_top(lua, 0);
}

static int projectiles_execute(void *lua, int hooked) {
    projectiles_count = projectiles_returns = 0;
    projectiles_return[0] = projectiles_return[1] = projectiles_return[2] = 0;
    projectiles_length = projectiles_input[10];
    projectiles_prepare(lua);
    damage_lua.get_field(lua, -10002, "CollisionCallbacks");
    damage_lua.get_field(lua, 1, "birdCollision");
    damage_lua.push_string(lua, "first");
    damage_lua.push_string(lua, "second");
    for (size_t field = 0; field < 4; ++field) {
        damage_lua.push_number(lua, 0);
    }
    damage_lua.push_nil(lua);
    damage_lua.push_nil(lua);
    if (hooked) {
        (void)projectiles_set_hook(lua, projectiles_return_hook, 2, 0);
    }
    int status = damage_lua.call(lua, 8, 0, 0);
    (void)projectiles_set_hook(lua, NULL, 0, 0);
    if (hooked && ((status == 0 && projectiles_returns != 1) ||
                   (status != 0 && projectiles_returns))) {
        fail("Original bird projectile return hook/status mismatch\n");
    }
    if (status) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        if (!message) {
            fail("Missing original bird projectile failure diagnostic\n");
        }
    }
    damage_lua.set_top(lua, 0);
    return status;
}

static void probe_bird_projectiles(void) {
    initialize_damage_lua();
    projectiles_raw_set_i =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    projectiles_raw_get_i =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    projectiles_set_hook =
        (int (*)(void *, void (*)(void *, void *), int, int))find_function(
            "_Z11lua_sethookP9lua_StatePFvS0_P9lua_DebugEii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird projectile state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_PROJECTILES_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_projectiles_case(sample, projectiles_input);
        print_words(projectiles_input, 25);
        int status = projectiles_execute(lua, 1);
        uint32_t result[] = {status != 0, status == 0 && projectiles_return[1] == 4,
                             status == 0 && projectiles_return[0] != 973};
        print_words(result, 3);
        print_words(projectiles_return, 3);
        uint32_t state[145];
        projectiles_snapshot(lua, state);
        print_words(state, 145);
        print_words(&projectiles_count, 1);
        for (uint32_t event = 0; event < projectiles_count; ++event) {
            print_words(projectiles_events[event], 148);
        }
        uint32_t captured_count = projectiles_count, captured_events[2][148];
        for (uint32_t event = 0; event < captured_count; ++event) {
            for (size_t field = 0; field < 148; ++field) {
                captured_events[event][field] = projectiles_events[event][field];
            }
        }
        int plain_status = projectiles_execute(lua, 0);
        uint32_t plain_state[145];
        projectiles_snapshot(lua, plain_state);
        if (status != plain_status || projectiles_count != captured_count) {
            fail("Original projectile hook changed status or event count\n");
        }
        for (size_t field = 0; field < 145; ++field) {
            if (state[field] != plain_state[field]) {
                fail("Original projectile hook changed captured state\n");
            }
        }
        for (uint32_t event = 0; event < captured_count; ++event) {
            for (size_t field = 0; field < 148; ++field) {
                if (captured_events[event][field] != projectiles_events[event][field]) {
                    fail("Original projectile hook changed boundary trace\n");
                }
            }
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird projectiles reference complete\n");
}

#endif
