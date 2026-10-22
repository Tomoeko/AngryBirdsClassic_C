#ifndef AB_ORACLE_BIRD_IMPACT_PREDICTION_H
#define AB_ORACLE_BIRD_IMPACT_PREDICTION_H

#include "../../tests/bird_impact_prediction_cases.h"
#include "prediction_graph_probe.h"

static uint32_t predict_native_input[PREDICT_INPUT_WORDS];
static uint32_t predict_native_events[PREDICT_MAX_EVENTS][PREDICT_EVENT_WORDS];
static uint32_t predict_native_count, predict_native_rays, predict_native_radii;
static uint32_t predict_native_printed;

static void predict_native_words(void *lua, uint32_t output[5]) {
    int top = damage_lua.get_top(lua);
    const char *const tables[] = {"_ab_predict_bird", "_ab_predict_velocity"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, tables[index]);
        damage_lua.get_field(lua, top + 1, "x");
        FloatBits x = {.number = damage_lua.to_number(lua, -1)};
        damage_lua.set_top(lua, top + 1);
        damage_lua.get_field(lua, top + 1, "y");
        FloatBits y = {.number = damage_lua.to_number(lua, -1)};
        output[index * 2] = x.bits;
        output[index * 2 + 1] = y.bits;
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "objects");
    damage_lua.get_field(lua, top + 2, "worldGravity");
    FloatBits gravity = {.number = damage_lua.to_number(lua, -1)};
    output[4] = gravity.bits;
    damage_lua.set_top(lua, top);
}

static uint32_t *predict_native_event(void *lua, uint32_t kind, uint32_t ordinal) {
    uint32_t size = 0, pc = predict_native_pc(lua, &size);
    int valid = kind == 1   ? size == 22 && pc == 19
                : kind == 2 ? size == 115 && (pc == 58 || pc == 73)
                            : size == 115 && pc == 109;
    int arguments = kind == 1 ? 4 : kind == 2 ? 2 : 1;
    if (!valid || damage_lua.get_top(lua) != arguments ||
        predict_native_count >= PREDICT_MAX_EVENTS) {
        fail("Unexpected original impact-prediction boundary\n");
    }
    uint32_t *event = predict_native_events[predict_native_count++];
    for (size_t index = 0; index < PREDICT_EVENT_WORDS; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    event[1] = ordinal;
    predict_native_words(lua, event + 6);
    return event;
}

static void predict_native_gravity(void *lua, uint32_t bits) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "objects");
    damage_number(lua, top + 2, "worldGravity", bits);
    damage_lua.set_top(lua, top);
}

static int predict_native_radius(void *lua) {
    uint32_t *event = predict_native_event(lua, 2, ++predict_native_radii);
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 501 || !damage_named(lua, 2, "radius")) {
        fail("Unexpected original impact-prediction radius receiver/key\n");
    }
    damage_lua.set_top(lua, 2);
    event[2] = 501;
    event[3] = 1;
    event[11] = predict_native_input[predict_native_radii & 1u ? 12 : 13];
    if (predict_native_radii == 1) {
        if (predict_native_input[1] & 1u) {
            damage_lua.get_field(lua, -10002, "_ab_predict_bird");
            damage_number(lua, 3, "x", predict_native_input[4] ^ UINT32_C(0x80000000));
            damage_number(lua, 3, "y", predict_native_input[5] ^ UINT32_C(0x80000000));
            damage_lua.set_top(lua, 2);
            damage_lua.get_field(lua, -10002, "_ab_predict_velocity");
            damage_number(lua, 3, "x", predict_native_input[9]);
            damage_number(lua, 3, "y", predict_native_input[10]);
            damage_lua.set_top(lua, 2);
            predict_native_gravity(lua, predict_native_input[11]);
        }
        if (predict_native_input[1] & 2u) {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, 1, "get");
        }
    }
    FloatBits radius = {.bits = event[11]};
    damage_lua.push_number(lua, radius.number);
    return 1;
}

static int predict_native_raycast(void *lua) {
    uint32_t *event = predict_native_event(lua, 1, ++predict_native_rays);
    for (int index = 1; index <= 4; ++index) {
        if (damage_lua.type(lua, index) != 3) {
            fail("Original impact-prediction ray coordinate is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        event[1 + (size_t)index] = value.bits;
    }
    event[11] =
        bird_impact_prediction_hit(predict_native_input[14], predict_native_rays);
    if (predict_native_rays == 1 && (predict_native_input[1] & 4u)) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 5, "raycast");
        damage_lua.set_top(lua, 4);
    }
    damage_lua.push_boolean(lua, event[11] != 0);
    const char *const names[] = {"first", "second", "third"};
    damage_lua.push_string(lua, names[predict_native_rays % 3u]);
    uint32_t point[2];
    bird_impact_prediction_point(predict_native_input[0], predict_native_rays, point);
    FloatBits x = {.bits = point[0]}, y = {.bits = point[1]};
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    return 4;
}

static int predict_native_print(void *lua) {
    uint32_t count = (uint32_t)damage_lua.to_number(lua, 1);
    if (count != predict_native_rays) {
        fail("Original impact-prediction query count mismatch\n");
    }
    (void)predict_native_event(lua, 3, count);
    predict_native_printed = count;
    predict_native_gravity(lua, predict_native_input[11] ^ UINT32_C(0x80000000));
    return 0;
}

static void predict_native_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 3);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 2, "worldGravity", predict_native_input[8]);
    damage_lua.set_field(lua, 1, "objects");
    damage_function(lua, 1, "raycast", predict_native_raycast);
    damage_lua.get_field(lua, -10002, "vectorPerpendicular");
    damage_lua.set_field(lua, 1, "vectorPerpendicular");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 1, "x", predict_native_input[4]);
    damage_number(lua, 1, "y", predict_native_input[5]);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, 501);
    damage_lua.set_field(lua, 2, "_id");
    damage_function(lua, 2, "get", predict_native_radius);
    damage_lua.set_field(lua, 1, "gameObjectScript");
    damage_lua.set_field(lua, -10002, "_ab_predict_bird");
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.get_field(lua, 1, "Vector2");
    damage_lua.get_field(lua, 2, "new");
    damage_lua.get_field(lua, 1, "Vector2");
    FloatBits x = {.bits = predict_native_input[6]},
              y = {.bits = predict_native_input[7]};
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    if (damage_lua.call(lua, 3, 1, 0)) {
        fail("Original prediction velocity construction failed\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_predict_velocity");
    damage_lua.set_top(lua, 0);
    if (predict_native_input[1] & 8u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "print");
    } else {
        damage_function(lua, -10002, "print", predict_native_print);
    }
}

static int predict_native_call_error(const char *message) {
    const char *wanted = "call";
    for (size_t start = 0; message && message[start]; ++start) {
        size_t index = 0;
        while (wanted[index] && message[start + index] == wanted[index]) {
            ++index;
        }
        if (!wanted[index]) {
            return 1;
        }
    }
    return 0;
}

static void probe_bird_impact_prediction(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird impact-prediction state\n");
    }
    load_prediction_graph(lua);
    semihost(4, "AB_ANDROID_BIRD_IMPACT_PREDICTION_V1 128\n");
    for (uint32_t sample = 0; sample < 128; ++sample) {
        bird_impact_prediction_case(sample, predict_native_input);
        print_words(predict_native_input, PREDICT_INPUT_WORDS);
        predict_native_count = predict_native_rays = predict_native_radii =
            predict_native_printed = 0;
        predict_native_prepare(lua);
        damage_lua.get_field(lua, -10002, "PhysicsQueries");
        damage_lua.get_field(lua, 1, "predictBirdImpactPosition");
        damage_lua.set_field(lua, -10002, "_ab_predict_function");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_predict_function");
        damage_lua.get_field(lua, -10002, "_ab_predict_bird");
        damage_lua.get_field(lua, -10002, "_ab_predict_velocity");
        if (predict_native_input[2] == 2) {
            FloatBits steps = {.bits = predict_native_input[3]};
            damage_lua.push_number(lua, steps.number);
        } else if (predict_native_input[2] == 1) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        uint32_t output[PREDICT_OUTPUT_WORDS] = {0};
        output[0] = damage_lua.call(lua, 3, 3, 0) != 0;
        if (output[0]) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (!(predict_native_input[1] & 14u) ||
                !predict_native_call_error(message)) {
                fail(message ? message : "Unknown original impact-prediction error\n");
            }
        }
        if (!output[0]) {
            output[1] = damage_lua.type(lua, 1) == 5;
            if (output[1]) {
                damage_lua.get_field(lua, 1, "x");
                FloatBits x = {.number = damage_lua.to_number(lua, -1)};
                damage_lua.set_top(lua, 3);
                damage_lua.get_field(lua, 1, "y");
                FloatBits y = {.number = damage_lua.to_number(lua, -1)};
                damage_lua.set_top(lua, 3);
                output[2] = x.bits;
                output[3] = y.bits;
            }
            output[4] = (uint32_t)damage_lua.type(lua, 2);
            if (output[4] == 4) {
                output[5] = damage_named(lua, 2, "first")    ? 1u
                            : damage_named(lua, 2, "second") ? 2u
                                                             : 3u;
            } else if (output[4] != 0) {
                fail("Unexpected original impact-prediction hit name\n");
            }
            FloatBits time = {.number = damage_lua.to_number(lua, 3)};
            output[6] = time.bits;
            output[7] = predict_native_printed;
        }
        damage_lua.set_top(lua, 0);
        output[8] = predict_native_count;
        predict_native_words(lua, output + 9);
        print_words(output, PREDICT_OUTPUT_WORDS);
        for (uint32_t event = 0; event < predict_native_count; ++event) {
            print_words(predict_native_events[event], PREDICT_EVENT_WORDS);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird impact prediction reference complete\n");
}

#endif
