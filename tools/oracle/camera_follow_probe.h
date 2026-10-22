#ifndef AB_ORACLE_CAMERA_FOLLOW_H
#define AB_ORACLE_CAMERA_FOLLOW_H

#include "../../tests/camera_follow_cases.h"
#include "prediction_graph_probe.h"

static uint32_t follow_native_input[FOLLOW_INPUT_WORDS];
static uint32_t follow_native_events[FOLLOW_MAX_EVENTS][FOLLOW_EVENT_WORDS];
static uint32_t follow_native_count, follow_native_ordinals[11];

static uint32_t follow_native_field(void *lua, int table, const char *field,
                                    uint32_t *present) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    int type = damage_lua.type(lua, -1);
    if (type != 0 && type != 3) {
        fail("Unexpected original follow-camera numeric field type\n");
    }
    *present = type == 3;
    FloatBits value = {.number = type == 3 ? damage_lua.to_number(lua, -1) : 0};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static uint32_t follow_native_tween_kind(void *lua, int table) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, "tweenFunction");
    int type = damage_lua.type(lua, -1);
    uint32_t kind = type == 6 ? 1u : type == 5 ? 2u : 0;
    if (type != 0 && !kind) {
        fail("Unexpected original follow-camera movement tween\n");
    }
    damage_lua.set_top(lua, top);
    return kind;
}

static void follow_native_values(void *lua, int table, uint32_t event[24]) {
    const char *const fields[] = {"x", "y", "zoom", "movementDuration", "zoomDuration"};
    for (uint32_t index = 0; index < 5; ++index) {
        uint32_t present = 0;
        event[5 + index] = follow_native_field(lua, table, fields[index], &present);
        event[4] |= present << index;
    }
    event[10] = follow_native_tween_kind(lua, table);
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, "zoomTweenFunction");
    event[11] = damage_lua.type(lua, -1) == 6;
    if (!event[11] && damage_lua.type(lua, -1) != 0) {
        fail("Unexpected original follow-camera zoom tween\n");
    }
    damage_lua.set_top(lua, top);
}

static uint32_t follow_native_pc(void *lua, uint32_t *size) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u) {
        fail("Missing original follow-camera caller\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original follow-camera closure\n");
    }
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || !prototype || (prototype[11] != 278 && prototype[11] != 21) ||
        closure[7] != (prototype[11] == 278 ? 1u : 0u)) {
        fail("Unexpected original follow-camera prototype\n");
    }
    *size = prototype[11];
    uintptr_t begin = prototype[3], saved = caller[3];
    const uint32_t *code = (const uint32_t *)begin;
    if (!begin || saved <= begin || saved > begin + *size * 4u ||
        (saved - begin) % 4u ||
        code[0] != (*size == 278 ? UINT32_C(0x0000009a) : UINT32_C(0x00000005)) ||
        code[*size - 1u] != UINT32_C(0x0080001e)) {
        fail("Invalid original follow-camera PC\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static void follow_native_vector(void *lua, uint32_t x, uint32_t y) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.get_field(lua, top + 1, "Vector2");
    damage_lua.get_field(lua, top + 2, "new");
    damage_lua.get_field(lua, top + 1, "Vector2");
    FloatBits first = {.bits = x}, second = {.bits = y};
    damage_lua.push_number(lua, first.number);
    damage_lua.push_number(lua, second.number);
    if (damage_lua.call(lua, 3, 1, 0)) {
        fail("Original follow-camera vector construction failed\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_follow_vector");
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "_ab_follow_vector");
}

static int follow_native_host(void *lua) {
    uint32_t kind = (uint32_t)damage_lua.to_number(lua, -10003);
    uint32_t size = 0, pc = follow_native_pc(lua, &size);
    int valid =
        kind == 1   ? size == 278 && pc == 18
        : kind == 2 ? size == 278 && (pc == 36 || pc == 130)
        : kind == 3
            ? (size == 278 && (pc == 46 || pc == 144)) || (size == 21 && pc == 9)
        : kind == 4 ? size == 278 && (pc == 62 || pc == 162 || pc == 171 || pc == 231)
        : kind == 5 ? size == 278 && pc == 68
        : kind == 6
            ? (size == 278 && (pc == 152 || pc == 238)) || (size == 21 && pc == 17)
        : kind == 7 ? size == 278 && pc == 163
        : kind == 8 ? size == 278 && (pc == 178 || pc == 204)
        : kind == 9 ? size == 278 && pc == 270
                    : kind == 10 && size == 21 && pc == 8;
    const int arguments[] = {0, 2, 2, 2, 1, 2, 3, 4, 2, 2, 1};
    if (!valid || kind > 10 || damage_lua.get_top(lua) != arguments[kind] ||
        follow_native_count >= FOLLOW_MAX_EVENTS) {
        fail("Unexpected original follow-camera boundary/arity\n");
    }
    uint32_t *event = follow_native_events[follow_native_count++];
    for (uint32_t index = 0; index < FOLLOW_EVENT_WORDS; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    event[1] = ++follow_native_ordinals[kind];
    event[2] = pc;
    event[3] = size;
    uint32_t ignored = 0;
    if (kind == 1) {
        damage_lua.get_field(lua, -10002, "_ab_follow_target");
        damage_lua.get_field(lua, 3, "launchVelocity");
        if (damage_lua.to_pointer(lua, 1) != damage_lua.to_pointer(lua, 3) ||
            damage_lua.to_pointer(lua, 2) != damage_lua.to_pointer(lua, 4)) {
            fail("Original follow camera changed prediction argument identity\n");
        }
        damage_lua.set_top(lua, 2);
        event[4] = 15;
        event[5] = follow_native_field(lua, 1, "x", &ignored);
        event[6] = follow_native_field(lua, 1, "y", &ignored);
        event[7] = follow_native_field(lua, 2, "x", &ignored);
        event[8] = follow_native_field(lua, 2, "y", &ignored);
    } else if (kind == 5 || kind == 6) {
        follow_native_values(lua, 2, event);
        if (kind == 6) {
            if (damage_lua.type(lua, 3) != 1 || !damage_lua.to_boolean(lua, 3)) {
                fail("Original follow camera dropped force-zoom selector\n");
            }
            event[12] = 1;
        }
    } else if (kind == 8) {
        event[4] = 3;
        event[5] = follow_native_field(lua, 2, "x", &ignored);
        event[6] = follow_native_field(lua, 2, "y", &ignored);
    } else if (kind == 2 || kind == 3 || kind == 7) {
        int first = kind == 2 ? 1 : 2;
        int count = kind == 2 ? 2 : kind == 3 ? 1 : 3;
        for (int index = 0; index < count; ++index) {
            if (damage_lua.type(lua, first + index) != 3) {
                fail("Original follow-camera numeric argument missing\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, first + index)};
            event[5 + index] = value.bits;
            event[4] |= 1u << (uint32_t)index;
        }
    } else if (kind == 9 && !damage_named(lua, 2, "FollowShotBirdEndCameraSequence")) {
        fail("Unexpected original follow-camera sequence name\n");
    }
    if (kind != 1 && kind != 2) {
        damage_lua.get_field(lua, 1, "_id");
        if (damage_lua.to_number(lua, -1) != 701) {
            fail("Unexpected original follow-camera service receiver\n");
        }
        damage_lua.set_top(lua, arguments[kind]);
    }
    camera_follow_response(follow_native_input, kind, event[1], event);
    if (kind == 1) {
        if (follow_native_input[1] & FOLLOW_NO_HIT) {
            damage_lua.push_nil(lua);
            damage_lua.push_nil(lua);
        } else {
            follow_native_vector(lua, event[13], event[14]);
            damage_lua.push_string(lua, "predicted-target");
        }
        FloatBits time = {.bits = event[15]};
        damage_lua.push_number(lua, time.number);
        return 3;
    }
    if (kind == 5 || kind == 6 || kind == 7) {
        damage_lua.create_table(lua, 0, 3);
        int result = damage_lua.get_top(lua);
        if (kind == 7) {
            damage_number(lua, result, "top", event[14]);
        } else {
            damage_number(lua, result, "x", event[13]);
            damage_number(lua, result, "y", event[14]);
            damage_number(lua, result, kind == 5 ? "id" : "zoom", event[15]);
        }
        return 1;
    }
    if (kind == 9) {
        return 0;
    }
    FloatBits first = {.bits = event[13]}, second = {.bits = event[14]};
    damage_lua.push_number(lua, first.number);
    if (kind == 2 || kind == 3) {
        damage_lua.push_number(lua, second.number);
        return 2;
    }
    return 1;
}

static void follow_native_bind(void *lua, int table, const char *name, uint32_t kind) {
    damage_lua.push_number(lua, (float)kind);
    damage_lua.push_closure(lua, follow_native_host, 1);
    damage_lua.set_field(lua, table, name);
}

static void follow_native_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 4);
    damage_number(lua, 1, "x", follow_native_input[2]);
    damage_number(lua, 1, "y", follow_native_input[3]);
    if (!(follow_native_input[1] & FOLLOW_NO_LAUNCH)) {
        follow_native_vector(lua, follow_native_input[4], follow_native_input[5]);
        damage_lua.set_field(lua, 1, "launchVelocity");
    }
    damage_lua.set_field(lua, -10002, "_ab_follow_target");
    damage_lua.create_table(lua, 0, 6);
    follow_native_bind(lua, 1, "physicsToWorldTransform", 2);
    const char *const tweens[] = {"easeSmoothStep", "easeCosineInOut",
                                  "tweenEaseCubicInOut"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, -10002, tweens[index]);
        damage_lua.set_field(lua, 1, tweens[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 2, "worldGravity", follow_native_input[6]);
    damage_lua.set_field(lua, 1, "objects");
    damage_lua.create_table(lua, 0, 10);
    damage_lua.push_number(lua, 701);
    damage_lua.set_field(lua, 2, "_id");
    const char *const methods[] = {
        "getProjectedScreenCoordinates",      "getZoomScale",
        "getClosestCameraSnapPoint",          "getClosestValidCameraValues",
        "getScreenExtentsForPositionAndZoom", "getZoomScaleToFitOnScreen",
        "startCameraSequenceFromFile",        "getScreenX"};
    for (uint32_t index = 0; index < 8; ++index) {
        follow_native_bind(lua, 2, methods[index], index + 3u);
    }
    damage_lua.set_field(lua, 1, "cameraSystem");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -10002, "PhysicsQueries");
    follow_native_bind(lua, 1, "predictBirdImpactPosition", 1);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "FollowBirdCameraMovementBehavior");
    damage_lua.get_field(lua, 1, "new");
    damage_lua.get_field(lua, -10002, "FollowBirdCameraMovementBehavior");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "_ab_follow_target");
    damage_lua.set_field(lua, 4, "targetObject");
    if (follow_native_input[1] & FOLLOW_COMPENSATION_PRESENT) {
        damage_number(lua, 4, "velocityCompensation", follow_native_input[29]);
    }
    if (damage_lua.call(lua, 2, 1, 0)) {
        fail("Original follow-camera construction failed\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_follow_state");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_follow_state");
    damage_lua.get_field(lua, 1, "setAllowCancel");
    damage_lua.get_field(lua, -10002, "_ab_follow_state");
    damage_lua.push_boolean(lua, (follow_native_input[1] & FOLLOW_ALLOW_CANCEL) != 0);
    if (damage_lua.call(lua, 2, 0, 0)) {
        fail("Original follow-camera cancellation selection failed\n");
    }
    if (follow_native_input[1] & FOLLOW_CACHED_HIT) {
        follow_native_vector(lua, follow_native_input[7], follow_native_input[8]);
        damage_lua.set_field(lua, 1, "hitPosition");
        damage_number(lua, 1, "hitTime", follow_native_input[9]);
    }
    if (follow_native_input[1] & FOLLOW_SCROLL_PRESENT) {
        damage_number(lua, 1, "scrollTimer", follow_native_input[27]);
    }
    if (follow_native_input[1] & FOLLOW_PEAK_PRESENT) {
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "peakIsHitPosition");
    }
    damage_lua.set_top(lua, 0);
}

static void follow_native_snapshot(void *lua, uint32_t output[FOLLOW_STATE_WORDS]) {
    damage_lua.get_field(lua, -10002, "_ab_follow_targets");
    if (damage_lua.type(lua, 1) == 5) {
        uint32_t fields[24] = {0};
        follow_native_values(lua, 1, fields);
        output[2] = 1;
        output[3] = fields[4];
        for (uint32_t index = 0; index < 5; ++index) {
            output[4 + index] = fields[5 + index];
        }
        output[9] = fields[10];
        output[10] = fields[11];
    } else if (damage_lua.type(lua, 1) != 0) {
        fail("Unexpected original follow-camera target result type\n");
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_follow_state");
    damage_lua.get_field(lua, 1, "hitPosition");
    uint32_t present = 0;
    if (damage_lua.type(lua, 2) == 5) {
        output[11] = 1;
        output[12] = follow_native_field(lua, 2, "x", &present);
        output[13] = follow_native_field(lua, 2, "y", &present);
    } else if (damage_lua.type(lua, 2) != 0) {
        fail("Unexpected original follow-camera retained hit type\n");
    }
    damage_lua.set_top(lua, 1);
    output[14] = follow_native_field(lua, 1, "hitTime", output + 25);
    output[17] = follow_native_field(lua, 1, "originalZoom", output + 16);
    output[19] = follow_native_field(lua, 1, "timeToPeak", output + 18);
    output[21] = follow_native_field(lua, 1, "scrollTimer", output + 20);
    output[22] = follow_native_field(lua, 1, "movementDurationScaler", &present);
    output[23] = follow_native_field(lua, 1, "velocityCompensation", &present);
    output[24] = follow_native_field(lua, 1, "minimumMovementDuration", &present);
    output[29] = follow_native_field(lua, 1, "timer", &present);
    damage_lua.get_field(lua, 1, "peakIsHitPosition");
    output[26] = damage_lua.type(lua, -1) != 0;
    output[15] = damage_lua.to_boolean(lua, -1) != 0;
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "allowCancel");
    output[27] = damage_lua.type(lua, -1) != 0;
    output[28] = damage_lua.to_boolean(lua, -1) != 0;
    damage_lua.set_top(lua, 0);
}

static void
follow_native_curves(void *lua,
                     uint32_t output[FOLLOW_CURVE_SAMPLES][FOLLOW_CURVE_WORDS]) {
    for (uint32_t sample = 0; sample < FOLLOW_CURVE_SAMPLES; ++sample) {
        for (uint32_t curve = 0; curve < 3; ++curve) {
            damage_lua.get_field(lua, -10002, "_ab_follow_targets");
            if (damage_lua.type(lua, 1) != 5) {
                damage_lua.set_top(lua, 0);
                continue;
            }
            damage_lua.get_field(lua, 1, curve ? "tweenFunction" : "zoomTweenFunction");
            if (curve && damage_lua.type(lua, 2) == 5) {
                damage_lua.get_field(lua, 2, curve == 1 ? "x" : "y");
            } else if (curve == 2) {
                damage_lua.set_top(lua, 0);
                continue;
            }
            if (damage_lua.type(lua, -1) != 6) {
                damage_lua.set_top(lua, 0);
                continue;
            }
            damage_lua.push_number(lua, camera_follow_curve_time(sample));
            damage_lua.push_number(lua, 1);
            damage_lua.push_number(lua, 2);
            damage_lua.push_number(lua, 3);
            if (damage_lua.call(lua, 4, 1, 0) || damage_lua.type(lua, -1) != 3) {
                fail("Original follow-camera captured tween evaluation failed\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[sample][curve * 2u] = 1;
            output[sample][curve * 2u + 1u] = value.bits;
            damage_lua.set_top(lua, 0);
        }
    }
}

static int follow_native_index_error(const char *message) {
    const char *wanted = "index";
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

static void probe_camera_follow(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original follow-camera state\n");
    }
    load_prediction_graph(lua);
    const char *const names[] = {
        "clamp",           "tweenLinear",         "smoothstep", "easeSmoothStep",
        "easeCosineInOut", "tweenEaseCubicInOut", "deepCopy"};
    const char *const paths[] = {
        "/data/local/tmp/ab520/camera/follow-clamp.lua",
        "/data/local/tmp/ab520/camera/follow-linear.lua",
        "/data/local/tmp/ab520/camera/follow-smoothstep.lua",
        "/data/local/tmp/ab520/camera/follow-ease-smoothstep.lua",
        "/data/local/tmp/ab520/camera/follow-cosine.lua",
        "/data/local/tmp/ab520/camera/follow-cubic.lua",
        "/data/local/tmp/ab520/camera/follow-copy.lua"};
    for (size_t index = 0; index < 7; ++index) {
        predict_native_load_function(lua, paths[index], names[index]);
    }
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/follow-automatic.lua");
    semihost(4, "AB_ANDROID_CAMERA_FOLLOW_V1 256 4 6\n");
    for (uint32_t sample = 0; sample < FOLLOW_CASES; ++sample) {
        camera_follow_case(sample, follow_native_input);
        print_words(follow_native_input, FOLLOW_INPUT_WORDS);
        for (uint32_t index = 0; index < 11; ++index) {
            follow_native_ordinals[index] = 0;
        }
        follow_native_prepare(lua);
        for (uint32_t update = 0; update < FOLLOW_UPDATES; ++update) {
            follow_native_count = 0;
            damage_lua.get_field(lua, -10002, "_ab_follow_state");
            damage_lua.get_field(lua, 1, "update");
            damage_lua.set_field(lua, -10002, "_ab_follow_update");
            damage_lua.set_top(lua, 0);
            damage_lua.get_field(lua, -10002, "_ab_follow_update");
            damage_lua.get_field(lua, -10002, "_ab_follow_state");
            damage_lua.push_number(lua, camera_follow_seconds(update));
            damage_lua.push_boolean(lua, update == 0 &&
                                             (follow_native_input[1] & FOLLOW_CANCEL));
            uint32_t output[FOLLOW_STATE_WORDS] = {0};
            output[0] = damage_lua.call(lua, 3, 2, 0) != 0;
            if (output[0]) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
                uint32_t flags = follow_native_input[1];
                if (!(flags & FOLLOW_CACHED_HIT) || !(flags & FOLLOW_NO_LAUNCH) ||
                    !follow_native_index_error(message)) {
                    fail(message ? message
                                 : "Unknown original follow-camera failure\n");
                }
                damage_lua.set_top(lua, 0);
                damage_lua.push_nil(lua);
            } else {
                if (damage_lua.type(lua, 2) != 1) {
                    fail("Original follow-camera completion flag is not Boolean\n");
                }
                output[1] = damage_lua.to_boolean(lua, 2) != 0;
                damage_lua.set_top(lua, 1);
            }
            damage_lua.set_field(lua, -10002, "_ab_follow_targets");
            follow_native_snapshot(lua, output);
            uint32_t curves[FOLLOW_CURVE_SAMPLES][FOLLOW_CURVE_WORDS] = {{0}};
            follow_native_curves(lua, curves);
            output[31] = follow_native_count;
            print_words(output, FOLLOW_STATE_WORDS);
            for (uint32_t index = 0; index < FOLLOW_CURVE_SAMPLES; ++index) {
                print_words(curves[index], FOLLOW_CURVE_WORDS);
            }
            for (uint32_t index = 0; index < follow_native_count; ++index) {
                print_words(follow_native_events[index], FOLLOW_EVENT_WORDS);
            }
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera follow reference complete\n");
}

#endif
