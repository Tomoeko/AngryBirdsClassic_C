#ifndef AB_ORACLE_BIRD_IMPACT_WORLD_H
#define AB_ORACLE_BIRD_IMPACT_WORLD_H

#include "../../tests/bird_impact_world_cases.h"
#include "prediction_graph_probe.h"
#include <stdbool.h>

static void *impact_world_native_game, *impact_world_native_wrapper;
static int (*impact_world_native_cast)(void *, void *);
static uint32_t impact_world_native_input[IMPACT_WORLD_INPUT_WORDS];
static uint32_t impact_world_native_events[IMPACT_WORLD_MAX_EVENTS]
                                          [IMPACT_WORLD_EVENT_WORDS];
static uint32_t impact_world_native_count, impact_world_native_rays;
static uint32_t impact_world_native_radii, impact_world_native_printed;

static uint32_t *impact_world_native_event(void *lua, uint32_t kind, uint32_t ordinal) {
    uint32_t size = 0, pc = predict_native_pc(lua, &size);
    bool valid = kind == 1   ? size == 22 && pc == 19
                 : kind == 2 ? size == 115 && (pc == 58 || pc == 73)
                             : size == 115 && pc == 109;
    int arguments = kind == 1 ? 4 : kind == 2 ? 2 : 1;
    if (!valid || damage_lua.get_top(lua) != arguments ||
        impact_world_native_count >= IMPACT_WORLD_MAX_EVENTS) {
        fail("Unexpected original coupled impact-prediction boundary\n");
    }
    uint32_t *event = impact_world_native_events[impact_world_native_count++];
    for (size_t index = 0; index < IMPACT_WORLD_EVENT_WORDS; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    event[1] = ordinal;
    return event;
}

static int impact_world_native_ray(void *lua) {
    uint32_t *event = impact_world_native_event(lua, 1, ++impact_world_native_rays);
    for (int index = 1; index <= 4; ++index) {
        if (damage_lua.type(lua, index) != 3) {
            fail("Original coupled prediction ray is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        event[1 + (uint32_t)index] = value.bits;
    }
    /* The original wrapper owns this very Lua state. Invoke the unmodified
     * native GameLua boundary with the original four query arguments; return
     * its actual one/six results to the unchanged prediction graph. */
    int count =
        impact_world_native_cast(impact_world_native_game, impact_world_native_wrapper);
    game_ray_native_result(lua, count, event + 6);
    return count;
}

static int impact_world_native_radius(void *lua) {
    uint32_t *event = impact_world_native_event(lua, 2, ++impact_world_native_radii);
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 501 || !damage_named(lua, 2, "radius")) {
        fail("Unexpected original coupled prediction radius receiver/key\n");
    }
    damage_lua.set_top(lua, 2);
    event[2] = impact_world_native_input[8];
    FloatBits radius = {.bits = event[2]};
    damage_lua.push_number(lua, radius.number);
    return 1;
}

static int impact_world_native_print(void *lua) {
    if (damage_lua.type(lua, 1) != 3 ||
        damage_lua.to_number(lua, 1) != (float)impact_world_native_rays) {
        fail("Unexpected original coupled prediction print count\n");
    }
    uint32_t *event = impact_world_native_event(lua, 3, impact_world_native_rays);
    impact_world_native_printed = event[1];
    return 0;
}

static void impact_world_native_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 3);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 2, "worldGravity", impact_world_native_input[6]);
    damage_lua.set_field(lua, 1, "objects");
    damage_function(lua, 1, "raycast", impact_world_native_ray);
    damage_lua.get_field(lua, -10002, "vectorPerpendicular");
    damage_lua.set_field(lua, 1, "vectorPerpendicular");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 1, "x", impact_world_native_input[2]);
    damage_number(lua, 1, "y", impact_world_native_input[3]);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, 501);
    damage_lua.set_field(lua, 2, "_id");
    damage_function(lua, 2, "get", impact_world_native_radius);
    damage_lua.set_field(lua, 1, "gameObjectScript");
    damage_lua.set_field(lua, -10002, "_ab_world_bird");
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.get_field(lua, 1, "Vector2");
    damage_lua.get_field(lua, 2, "new");
    damage_lua.get_field(lua, 1, "Vector2");
    FloatBits x = {.bits = impact_world_native_input[4]};
    FloatBits y = {.bits = impact_world_native_input[5]};
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    if (damage_lua.call(lua, 3, 1, 0)) {
        fail("Original coupled prediction velocity construction failed\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_world_velocity");
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "print", impact_world_native_print);
}

static void probe_bird_impact_world_queries(void *lua, uint32_t scene) {
    for (uint32_t profile = 0; profile < IMPACT_WORLD_CASES; ++profile) {
        bird_impact_world_case(scene, profile, impact_world_native_input);
        print_words(impact_world_native_input, IMPACT_WORLD_INPUT_WORDS);
        impact_world_native_count = impact_world_native_rays =
            impact_world_native_radii = impact_world_native_printed = 0;
        impact_world_native_prepare(lua);
        damage_lua.get_field(lua, -10002, "PhysicsQueries");
        damage_lua.get_field(lua, 1, "predictBirdImpactPosition");
        damage_lua.set_field(lua, -10002, "_ab_world_predict");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_world_predict");
        damage_lua.get_field(lua, -10002, "_ab_world_bird");
        damage_lua.get_field(lua, -10002, "_ab_world_velocity");
        if (impact_world_native_input[9]) {
            FloatBits steps = {.bits = impact_world_native_input[7]};
            damage_lua.push_number(lua, steps.number);
        } else {
            damage_lua.push_nil(lua);
        }
        uint32_t output[IMPACT_WORLD_OUTPUT_WORDS] = {0};
        output[0] = damage_lua.call(lua, 3, 3, 0) != 0;
        if (output[0]) {
            fail("Original coupled prediction unexpectedly failed\n");
        }
        if (damage_lua.type(lua, 1) == 5) {
            output[1] = 1;
            damage_lua.get_field(lua, 1, "x");
            FloatBits x = {.number = damage_lua.to_number(lua, -1)};
            damage_lua.set_top(lua, 3);
            damage_lua.get_field(lua, 1, "y");
            FloatBits y = {.number = damage_lua.to_number(lua, -1)};
            damage_lua.set_top(lua, 3);
            output[8] = x.bits;
            output[9] = y.bits;
            if (damage_lua.type(lua, 2) == 4) {
                unsigned length = 0;
                const char *name = damage_lua.to_string(lua, 2, &length);
                if (!name || length > 16) {
                    fail("Invalid original coupled prediction name\n");
                }
                output[2] = 1;
                output[3] = length;
                game_raycast_pack(name, length, output + 4);
            } else if (damage_lua.type(lua, 2) != 0) {
                fail("Unexpected original coupled prediction name type\n");
            }
        } else if (damage_lua.type(lua, 1) != 0 || damage_lua.type(lua, 2) != 0) {
            fail("Unexpected original coupled prediction miss results\n");
        }
        if (damage_lua.type(lua, 3) != 3) {
            fail("Missing original coupled prediction time\n");
        }
        FloatBits time = {.number = damage_lua.to_number(lua, 3)};
        output[10] = time.bits;
        output[11] = impact_world_native_rays;
        output[12] = impact_world_native_radii;
        output[13] = impact_world_native_printed;
        output[14] = impact_world_native_count;
        print_words(output, IMPACT_WORLD_OUTPUT_WORDS);
        for (uint32_t event = 0; event < impact_world_native_count; ++event) {
            print_words(impact_world_native_events[event], IMPACT_WORLD_EVENT_WORDS);
        }
        damage_lua.set_top(lua, 0);
    }
}

#endif
