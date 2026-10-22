#ifndef AB_ORACLE_CAMERA_FOLLOW_SYSTEM_H
#define AB_ORACLE_CAMERA_FOLLOW_SYSTEM_H

#include "../../tests/camera_follow_system_cases.h"
#include "prediction_graph_probe.h"

static uint32_t owner_native_config[OWNER_FOLLOW_CONFIG_WORDS];
static uint32_t owner_native_command[OWNER_FOLLOW_COMMAND_WORDS];
static uint32_t owner_native_publication[4], owner_native_counts[12];
static uint32_t owner_native_events[OWNER_FOLLOW_MAX_EVENTS][OWNER_FOLLOW_EVENT_WORDS];
static uint32_t owner_native_count;

static void owner_native_checked(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original follow-camera owner graph failed\n");
    }
}

static uint32_t owner_native_number(void *lua, int table, const char *field) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != 3)
        fail("Missing original owner numeric field\n");
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static uint32_t owner_native_argument(void *lua, int index) {
    if (damage_lua.type(lua, index) != 3)
        fail("Invalid original owner numeric argument\n");
    FloatBits value = {.number = damage_lua.to_number(lua, index)};
    return value.bits;
}

static void owner_native_identity(void *lua, int index, const char *global) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, global);
    if (damage_lua.to_pointer(lua, index) != damage_lua.to_pointer(lua, -1))
        fail("Original owner boundary changed receiver identity\n");
    damage_lua.set_top(lua, top);
}

static uint32_t *owner_native_event(void *lua, uint32_t kind, uint32_t pc) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u ||
        owner_native_count >= OWNER_FOLLOW_MAX_EVENTS)
        fail("Invalid original owner caller\n");
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0])
        fail("Missing original owner closure\n");
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (!prototype)
        fail("Missing original owner prototype\n");
    uint32_t size = kind == 1 || kind >= 5 ? 59u : 278u;
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    if (kind == 4) {
        size = 19;
    } else if (kind == 7) {
        size = prototype[11];
        if (size != 19 && size != 24)
            fail("Unexpected original sequence print caller\n");
    }
#endif
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    if (kind == 8)
        size = 48;
#endif
    if (closure[6] || closure[7] != (size == 278 ? 1u : 0u) || !prototype ||
        prototype[11] != size || !prototype[3])
        fail("Unexpected original owner caller prototype\n");
    uint32_t begin = prototype[3], saved = caller[3];
    if (saved <= begin || saved > begin + size * 4u || (saved - begin) % 4u)
        fail("Invalid original owner caller PC\n");
    uint32_t actual_pc = (saved - begin) / 4u - 1u;
    if (pc == UINT32_MAX) {
        int valid_site = kind == 3 && (actual_pc == 36 || actual_pc == 130);
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
        if (kind == 7)
            valid_site = actual_pc == (size == 19 ? 15u : 3u);
#endif
        if (!valid_site)
            fail("Unexpected original owner transform site\n");
        pc = actual_pc;
    } else if (actual_pc != pc) {
        fail("Unexpected original owner caller PC\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uint32_t first = size == 59 ? UINT32_C(0x00400086) : UINT32_C(0x0000009a);
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    if (size == 19)
        first = UINT32_C(0x0000008a);
    else if (size == 24)
        first = UINT32_C(0x00000105);
#endif
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    if (size == 48)
        first = UINT32_C(0x00000045);
#endif
    if (code[0] != first || code[size - 1u] != UINT32_C(0x0080001e))
        fail("Unexpected original owner instructions\n");
    uint32_t *event = owner_native_events[owner_native_count++];
    for (uint32_t index = 0; index < OWNER_FOLLOW_EVENT_WORDS; ++index)
        event[index] = 0;
    event[0] = kind;
    event[1] = ++owner_native_counts[kind];
    event[2] = pc;
    event[3] = size;
    return event;
}

static void owner_native_vector(void *lua, float x, float y) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.get_field(lua, top + 1, "Vector2");
    damage_lua.get_field(lua, top + 2, "new");
    damage_lua.get_field(lua, top + 1, "Vector2");
    damage_lua.push_number(lua, x);
    damage_lua.push_number(lua, y);
    owner_native_checked(lua, 3, 1);
    damage_lua.set_field(lua, -10002, "_ab_owner_vector");
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "_ab_owner_vector");
}

static int owner_native_load_unused(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 4 ||
        !damage_named(lua, 2, ""))
        fail("Unexpected original owner unused resource boundary\n");
    return 0;
}

static int owner_native_input(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected original owner input arity\n");
    uint32_t *event = owner_native_event(lua, 1, 3);
    owner_native_identity(lua, 1, "_ab_owner_handler");
    event[4] = 501;
    event[6] = owner_native_argument(lua, 2);
    event[10] = owner_native_command[2];
    event[11] = owner_native_command[3];
    damage_lua.get_field(lua, -10002, "_ab_owner_input");
    damage_lua.push_boolean(lua, (owner_native_command[2] & 1u) != 0);
    return 2;
}

static int owner_native_top_left(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected original owner top-left arity\n");
    uint32_t *event = owner_native_event(lua, 5, 51);
    event[6] = owner_native_argument(lua, 1);
    event[7] = owner_native_argument(lua, 2);
    owner_native_publication[0] = event[6];
    owner_native_publication[1] = event[7];
    ++owner_native_publication[3];
    return 0;
}

static int owner_native_zoom(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected original owner zoom arity\n");
    uint32_t *event = owner_native_event(lua, 6, 57);
    event[6] = owner_native_argument(lua, 1);
    owner_native_publication[2] = event[6];
    ++owner_native_publication[3];
    return 0;
}

#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
static int owner_world_native_predict_result(void *lua, uint32_t *event);
#endif

static int owner_native_predict(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 5)
        fail("Unexpected original owner prediction arity/type\n");
    uint32_t *event = owner_native_event(lua, 2, 18);
    FloatBits id = {.bits = owner_native_number(lua, 1, "_id")};
    if (id.number != 1 && id.number != 2)
        fail("Invalid original owner predicted bird\n");
    event[4] = (uint32_t)id.number;
    owner_native_identity(lua, 1,
                          event[4] == 1 ? "_ab_owner_bird1" : "_ab_owner_bird2");
    damage_lua.get_field(lua, 1, "launchVelocity");
    if (damage_lua.to_pointer(lua, 2) != damage_lua.to_pointer(lua, 3))
        fail("Original owner prediction changed launch identity\n");
    damage_lua.set_top(lua, 2);
    event[6] = owner_native_number(lua, 1, "x");
    event[7] = owner_native_number(lua, 1, "y");
    event[8] = owner_native_number(lua, 2, "x");
    event[9] = owner_native_number(lua, 2, "y");
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    return owner_world_native_predict_result(lua, event);
#else
    event[10] = owner_native_config[13];
    event[11] = owner_native_config[14];
    event[12] = owner_native_config[15];
    event[13] = !(owner_native_config[18] & 1u);
    if (event[13]) {
        owner_native_vector(lua, owner_follow_number(event[10]),
                            owner_follow_number(event[11]));
        damage_lua.push_string(lua, "predicted-target");
    } else {
        damage_lua.push_nil(lua);
        damage_lua.push_nil(lua);
    }
    damage_lua.push_number(lua, owner_follow_number(event[12]));
    return 3;
#endif
}

static int owner_native_transform(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected original owner transform arity\n");
    uint32_t *event = owner_native_event(lua, 3, UINT32_MAX);
    event[6] = owner_native_argument(lua, 1);
    event[7] = owner_native_argument(lua, 2);
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    typedef void (*Transform)(FloatBits[2], const void *, float, float);
    Transform transform = (Transform)find_function(
        "_ZNK20CoordinateTransforms23physicsToWorldTransformEff");
    FloatBits transformed[2];
    transform(transformed, NULL, owner_follow_number(event[6]),
              owner_follow_number(event[7]));
    float x = transformed[0].number, y = transformed[1].number;
#else
    float scale = owner_follow_number(owner_native_config[17]);
    float x = owner_follow_number(event[6]) * scale;
    float y = owner_follow_number(event[7]) * scale;
#endif
    event[8] = owner_follow_word(x);
    event[9] = owner_follow_word(y);
    damage_lua.push_number(lua, x);
    damage_lua.push_number(lua, y);
    return 2;
}

#if !defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
static int owner_native_sequence(void *lua) {
    if (damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 2, "FollowShotBirdEndCameraSequence"))
        fail("Unexpected original owner terminal sequence boundary\n");
    uint32_t *event = owner_native_event(lua, 4, 270);
    owner_native_identity(lua, 1, "cameraSystem");
    event[4] = 1;
    event[6] = 1;
    return 0;
}

#else
static uint32_t owner_native_optional(void *lua, int table, const char *field,
                                      uint32_t *present);
#include "camera_follow_end_sequence_probe.h"
#endif

#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
#include "camera_follow_world_probe.h"
#endif

static void owner_native_load_graph(void *lua) {
    load_prediction_graph(lua);
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    damage_lua.get_field(lua, -10002, "PhysicsQueries");
    damage_lua.get_field(lua, 1, "predictBirdImpactPosition");
    damage_lua.set_field(lua, -10002, "_ab_owner_world_predict");
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "raycast", owner_world_native_ray);
#endif
    const char *const names[] = {"clamp",
                                 "tweenLinear",
                                 "smoothstep",
                                 "easeSmoothStep",
                                 "easeCosineInOut",
                                 "tweenEaseCubicInOut",
                                 "deepCopy",
                                 "vectorProjection",
                                 "vLength",
                                 "vectorLengthSquared",
                                 "vectorLength",
                                 "vLengthsq",
                                 "_ab_owner_default",
                                 "vectorDistance",
                                 "vectorSubtract"};
    const char *const paths[] = {
        "/data/local/tmp/ab520/camera/follow-clamp.lua",
        "/data/local/tmp/ab520/camera/follow-linear.lua",
        "/data/local/tmp/ab520/camera/follow-smoothstep.lua",
        "/data/local/tmp/ab520/camera/follow-ease-smoothstep.lua",
        "/data/local/tmp/ab520/camera/follow-cosine.lua",
        "/data/local/tmp/ab520/camera/follow-cubic.lua",
        "/data/local/tmp/ab520/camera/follow-copy.lua",
        "/data/local/tmp/ab520/camera/owner-projection.lua",
        "/data/local/tmp/ab520/camera/owner-length.lua",
        "/data/local/tmp/ab520/camera/owner-vector-squared.lua",
        "/data/local/tmp/ab520/camera/owner-vector-length.lua",
        "/data/local/tmp/ab520/camera/owner-squared.lua",
        "/data/local/tmp/ab520/camera/owner-default.lua",
        "/data/local/tmp/ab520/camera/owner-vector-distance.lua",
        "/data/local/tmp/ab520/camera/owner-vector-subtract.lua"};
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        predict_native_load_function(lua, paths[index], names[index]);
    }
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.get_field(lua, -10002, "_ab_owner_default");
    damage_lua.set_field(lua, 1, "default");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.push_string(lua, "scripts_common");
    damage_lua.set_field(lua, -10002, "commonScriptPath");
    damage_function(lua, -10002, "loadLuaFile", owner_native_load_unused);
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/owner-mover.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/owner-sequence.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/follow-automatic.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/owner-limits.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/owner-system.lua");
#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    owner_world_native_properties(lua);
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    damage_lua.push_string(lua, "scripts");
    damage_lua.set_field(lua, -10002, "scriptPath");
    damage_function(lua, -10002, "loadLuaFileToObject", owner_end_native_load);
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    damage_function(lua, -10002, "print", owner_world_native_print);
#else
    damage_function(lua, -10002, "print", owner_end_native_print);
#endif
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    damage_function(lua, -10002, "getObject", owner_shot_native_object);
    predict_native_load_function(lua,
                                 "/data/local/tmp/ab520/camera/bird-camera-launch.lua",
                                 "_ab_bird_camera_launch");
    predict_native_load_function(
        lua, "/data/local/tmp/ab520/camera/museum-camera-launch.lua",
        "_ab_museum_camera_launch");
    predict_native_load_function(lua,
                                 "/data/local/tmp/ab520/camera/blues-camera-init.lua",
                                 "_ab_blues_camera_init");
#endif
#endif
}

#include "camera_follow_system_input_probe.h"
#include "camera_follow_system_state_probe.h"

static int owner_native_nil_error(const char *message) {
    const char *wanted = "nil";
    for (size_t start = 0; message && message[start]; ++start) {
        size_t index = 0;
        while (wanted[index] && message[start + index] == wanted[index])
            ++index;
        if (!wanted[index])
            return 1;
    }
    return 0;
}

static void probe_camera_follow_system(void) {
    initialize_damage_lua();
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    void *lua = owner_world_native_state();
#else
    void *lua = damage_lua.new_state();
#endif
    if (!lua)
        fail("Cannot allocate original follow-camera owner state\n");
    owner_native_load_graph(lua);
#if defined(AB_ORACLE_CLASSIC_CAMERA_LIVE)
    semihost(4, "AB_ANDROID_CLASSIC_CAMERA_V2 48 160 3\n");
#elif defined(AB_ORACLE_CLASSIC_CAMERA)
    semihost(4, "AB_ANDROID_CLASSIC_CAMERA_V1 48 160 3\n");
#elif defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    semihost(4, "AB_ANDROID_CAMERA_FOLLOW_PROPERTIES_V1 48 160 12\n");
#elif defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    semihost(4, "AB_ANDROID_CAMERA_FOLLOW_WORLD_V1 48 160 12\n");
#elif defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    semihost(4, "AB_ANDROID_CAMERA_SHOT_SEQUENCE_V1 16 160\n");
#elif defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    semihost(4, "AB_ANDROID_CAMERA_FOLLOW_END_SEQUENCE_V1 16 160\n");
#else
    semihost(4, "AB_ANDROID_CAMERA_FOLLOW_SYSTEM_V1 16 32\n");
#endif
    for (uint32_t scene = 0; scene < OWNER_FOLLOW_SCENES; ++scene) {
        camera_follow_system_config(scene, owner_native_config);
        print_words(owner_native_config, OWNER_FOLLOW_CONFIG_WORDS);
        for (uint32_t index = 0; index < 12; ++index)
            owner_native_counts[index] = 0;
        for (uint32_t index = 0; index < 4; ++index)
            owner_native_publication[index] = 0;
        owner_native_count = 0;
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
        owner_world_native_open(owner_native_config[0]);
#endif
        owner_native_prepare(lua);
        for (uint32_t step = 0; step < OWNER_FOLLOW_STEPS; ++step) {
            camera_follow_system_command(scene, step, owner_native_command);
            print_words(owner_native_command, OWNER_FOLLOW_COMMAND_WORDS);
            owner_native_count = 0;
            owner_native_prepare_input(lua);
            owner_native_action(lua);
#if defined(AB_ORACLE_CLASSIC_CAMERA_LIVE)
            owner_classic_native_publish(lua);
#endif
            damage_lua.get_field(lua, -10002, "cameraSystem");
            damage_lua.get_field(lua, 1, "updateCamera");
            damage_lua.get_field(lua, -10002, "cameraSystem");
            damage_lua.push_number(lua, owner_follow_number(owner_native_command[1]));
            uint32_t output[OWNER_FOLLOW_OUTPUT_WORDS] = {0};
            output[0] = damage_lua.call(lua, 2, 0, 0) != 0;
            if (output[0]) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
                if (!owner_native_nil_error(message))
#else
                if (scene > 1 || !owner_native_nil_error(message))
#endif
                    fail(message ? message : "Unknown original owner update failure\n");
            }
            damage_lua.set_top(lua, 0);
            owner_native_snapshot(lua, output);
#if defined(AB_ORACLE_CLASSIC_CAMERA)
            owner_classic_native_compact(output);
#endif
            print_words(output, OWNER_FOLLOW_OUTPUT_WORDS);
            for (uint32_t index = 0; index < owner_native_count; ++index)
                print_words(owner_native_events[index], OWNER_FOLLOW_EVENT_WORDS);
        }
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
        owner_world_native_close();
#endif
    }
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    owner_world_native_release();
#else
    damage_lua.close_state(lua);
#endif
#if defined(AB_ORACLE_CLASSIC_CAMERA)
    semihost(4, "AB classic camera reference complete\n");
#elif defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    semihost(4, "AB camera follow properties reference complete\n");
#elif defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
    semihost(4, "AB camera follow world reference complete\n");
#elif defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    semihost(4, "AB camera shot sequence reference complete\n");
#elif defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    semihost(4, "AB camera follow end sequence reference complete\n");
#else
    semihost(4, "AB camera follow system reference complete\n");
#endif
}

#endif
