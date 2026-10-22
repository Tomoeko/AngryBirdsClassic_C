#ifndef AB_ORACLE_BIRD_DAMAGE_H
#define AB_ORACLE_BIRD_DAMAGE_H

#include <stdbool.h>

static uint32_t bird_damage_input[15];

static int bird_damage_property(void *lua) {
    uint32_t flags = bird_damage_input[8];
    if (damage_named(lua, 2, "velocityMultiplierModifier")) {
        if (flags & 128) {
            damage_lua.push_boolean(lua, 0);
        } else {
            FloatBits value = {.bits = bird_damage_input[7]};
            damage_lua.push_number(lua, value.number);
        }
    } else if (damage_named(lua, 2, "defence")) {
        if (flags & 256) {
            damage_lua.push_nil(lua);
        } else {
            FloatBits value = {.bits = bird_damage_input[4]};
            damage_lua.push_number(lua, value.number);
        }
    } else {
        damage_lua.push_boolean(lua, damage_named(lua, 2, "ignoreAllDamage") &&
                                         (flags & 16));
    }
    return 1;
}

static int bird_damage_definition(void *lua) {
    bool bird = damage_object_id(lua, 1) == 1;
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, bird ? "factors" : "target");
    damage_lua.set_field(lua, -2, bird ? "damageFactors" : "materialName");
    if (bird) {
        damage_lua.push_boolean(lua, (bird_damage_input[8] & 1) != 0);
        damage_lua.set_field(lua, -2, "useLegacyCollisionPath");
    }
    return 1;
}

static int bird_damage_velocity(void *lua) {
    if (damage_event_count >= 3) {
        fail("Bird velocity boundary event overflow\n");
    }
    uint32_t *event = damage_events[damage_event_count++];
    event[0] = 3;
    event[1] = damage_object_id(lua, 1);
    FloatBits horizontal = {.number = damage_lua.to_number(lua, 2)};
    FloatBits vertical = {.number = damage_lua.to_number(lua, 3)};
    event[5] = horizontal.bits;
    event[6] = vertical.bits;
    return 0;
}

static int bird_damage_callback(void *lua) {
    int result = damage_block_callback(lua);
    damage_events[damage_event_count - 1][0] = 4;
    return result;
}

static void prepare_bird_damage_objects(void *lua) {
    uint32_t flags = bird_damage_input[8];
    for (unsigned id = 1; id <= 2; ++id) {
        damage_lua.create_table(lua, 0, 6);
        int object = damage_lua.get_top(lua);
        damage_lua.push_string(lua, id == 1 ? "first" : "second");
        damage_lua.set_field(lua, object, "name");
        if (id == 1) {
            damage_number(lua, object, "xVel", bird_damage_input[0]);
            damage_number(lua, object, "yVel", bird_damage_input[1]);
            damage_number(lua, object, "mass", bird_damage_input[2]);
        } else if (!(flags & 8)) {
            damage_number(lua, object, "strength", bird_damage_input[3]);
        }
        if (flags & (id == 1 ? 2u : 4u)) {
            damage_lua.create_table(lua, 0, 1);
            damage_function(lua, -2, "get", bird_damage_property);
            damage_lua.set_field(lua, object, "gameObjectScript");
        }
        damage_lua.set_field(lua, -10002, id == 1 ? "first" : "second");
    }
}

static void prepare_bird_damage_game(void *lua) {
    damage_lua.create_table(lua, 0, 5);
    int game = damage_lua.get_top(lua);
    damage_function(lua, game, "getObjectDefinition", bird_damage_definition);
    damage_function(lua, game, "destroyBreakableJoints", damage_break_joints);
    damage_function(lua, game, "setVelocity", bird_damage_velocity);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, game, "deadBlocks");
    damage_lua.create_table(lua, 0, 1);
    int blocks = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 1);
    int factors = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 2);
    int factor = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 1);
    if (!(bird_damage_input[8] & 32)) {
        damage_number(lua, -2, "target", bird_damage_input[5]);
    }
    damage_lua.set_field(lua, factor, "damageMultiplier");
    damage_lua.create_table(lua, 0, 1);
    if (!(bird_damage_input[8] & 64)) {
        damage_number(lua, -2, "target", bird_damage_input[6]);
    }
    damage_lua.set_field(lua, factor, "velocityMultiplier");
    damage_lua.set_field(lua, factors, "factors");
    damage_lua.set_field(lua, blocks, "damageFactors");
    damage_lua.set_field(lua, game, "blockTable");
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void print_bird_damage_results(void *lua) {
    uint32_t result[7] = {0};
    result[3] = damage_lua.to_boolean(lua, 1) != 0;
    result[4] = (uint32_t)damage_lua.type(lua, 2);
    if (result[4] == 5) {
        damage_lua.get_field(lua, 2, "afterCollisionVelocityX");
        FloatBits horizontal = {.number = damage_lua.to_number(lua, -1)};
        result[5] = horizontal.bits;
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "afterCollisionVelocityY");
        FloatBits vertical = {.number = damage_lua.to_number(lua, -1)};
        result[6] = vertical.bits;
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "second");
    damage_lua.get_field(lua, -1, "strength");
    result[0] = (uint32_t)damage_lua.type(lua, -1);
    FloatBits strength = {.number = damage_lua.to_number(lua, -1)};
    result[1] = strength.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -1, "deadBlocks");
    damage_lua.get_field(lua, -1, "second");
    result[2] = damage_lua.type(lua, -1) == 5;
    damage_lua.set_top(lua, 0);
    print_words(result, 7);
    uint32_t count = (uint32_t)damage_event_count;
    print_words(&count, 1);
    for (size_t event = 0; event < damage_event_count; ++event) {
        print_words(damage_events[event], 9);
    }
}

#if defined(AB_ORACLE_BIRD_ROUTE_ONLY)
#include "bird_route_probe.h"
#endif
#if defined(AB_ORACLE_SLINGSHOT_ONLY)
#include "slingshot_probe.h"
#endif

static void probe_bird_damage(void) {
#if defined(AB_ORACLE_SLINGSHOT_ONLY)
    probe_slingshot();
    return;
#endif
#if defined(AB_ORACLE_BIRD_ROUTE_ONLY)
    probe_bird_route();
    return;
#endif
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate bird collision reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    damage_function(lua, -10002, "requireFile", damage_require);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/vector2.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/museum-collisions.lua");
    damage_lua.get_field(lua, -10002, "MuseumCollisions");
    damage_lua.get_field(lua, -1, "onCollision");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || damage_lua.type(lua, -1) != 6 || header[6] || header[7] != 4) {
        fail("Missing original bird collision closure\n");
    }
    const uint32_t *upvalue = (const uint32_t *)(uintptr_t)closure[6];
    const uint32_t *closure_value = (const uint32_t *)(uintptr_t)upvalue[2];
    if (closure_value[1] != 6) {
        fail("Original bird collision upvalue is not a function\n");
    }
    damage_lua.push_nil(lua);
    uint32_t *top = (uint32_t *)(uintptr_t)((const uint32_t *)lua)[2];
    top[-2] = closure_value[0];
    top[-1] = closure_value[1];
    damage_lua.set_field(lua, -10002, "_ab_bird_collision");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "birdCollision", bird_damage_callback);
    damage_lua.set_field(lua, -10002, "CollisionCallbacks");
    uint32_t state = UINT32_C(0x520b17d5);
    semihost(4, "AB_ANDROID_BIRD_DAMAGE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        bird_damage_input[0] = random_float(&state, 122, 12, 1);
        bird_damage_input[1] = random_float(&state, 122, 12, 1);
        bird_damage_input[2] = index % 17 ? random_float(&state, 123, 9, 0) : 0;
        bird_damage_input[3] = index % 31  ? random_float(&state, 114, 17, 0)
                               : index % 2 ? UINT32_C(0x80000000)
                                           : 0;
        bird_damage_input[4] = random_float(&state, 115, 14, 0);
        bird_damage_input[5] = index % 19 ? random_float(&state, 124, 7, 0) : 0;
        bird_damage_input[6] = random_float(&state, 123, 7, index % 11 == 0);
        bird_damage_input[7] = random_float(&state, 123, 6, index % 13 == 0);
        uint32_t flags = (uint32_t)(index & 511);
        if ((flags & 2) && !(flags & 4) && !(flags & 8)) {
            flags &= ~UINT32_C(2);
        }
        bird_damage_input[8] = flags;
        for (size_t field = 9; field < 15; ++field) {
            bird_damage_input[field] = random_float(&state, 120, 10, 1);
        }
        prepare_bird_damage_objects(lua);
        prepare_bird_damage_game(lua);
        damage_event_count = 0;
        for (size_t event = 0; event < 3; ++event) {
            for (size_t field = 0; field < 9; ++field) {
                damage_events[event][field] = 0;
            }
        }
        print_words(bird_damage_input, 15);
        damage_lua.get_field(lua, -10002, "_ab_bird_collision");
        damage_lua.get_field(lua, -10002, "first");
        damage_lua.get_field(lua, -10002, "second");
        for (size_t field = 9; field < 15; ++field) {
            FloatBits value = {.bits = bird_damage_input[field]};
            damage_lua.push_number(lua, value.number);
        }
        if (damage_lua.call(lua, 8, 2, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original bird collision failed\n");
        }
        print_bird_damage_results(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird damage reference complete\n");
}

#endif
