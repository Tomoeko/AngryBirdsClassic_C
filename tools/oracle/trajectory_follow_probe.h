#ifndef AB_ORACLE_TRAJECTORY_FOLLOW_H
#define AB_ORACLE_TRAJECTORY_FOLLOW_H

#include "../../tests/trajectory_follow_cases.h"

static void (*tf_push_value)(void *, int);
static void (*tf_get_index)(void *, int, int);
static unsigned (*tf_length)(void *, int);
static uint32_t tf_count, tf_serial, tf_events[8][8];
static const void *tf_objects[3];
static const char *const tf_names[] = {"_ab_tf_object1", "_ab_tf_object2",
                                       "_ab_tf_object3"};
static const char *const tf_records[] = {
    "_ab_tf_point1", "_ab_tf_point2", "_ab_tf_point3", "_ab_tf_point4",
    "_ab_tf_point5", "_ab_tf_point6", "_ab_tf_point7", "_ab_tf_point8"};
static const char *const tf_sprites[] = {"TRAIL_WHITE_1", "TRAIL_WHITE_2",
                                         "TRAIL_WHITE_3", "ALT_1", "ALT_2"};

static void tf_load(void *lua, const char *path, const char *name) {
    char bytes[1024];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, bytes, sizeof(bytes));
    if (size <= 0 || size == sizeof(bytes) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, bytes, (unsigned)size, "original-trajectory-helper"))
        fail("Cannot load original trajectory helper\n");
    damage_lua.set_field(lua, -10002, name);
}

static int tf_transform(void *lua) {
    typedef void (*Transform)(FloatBits[2], const void *, float, float);
    Transform transform = (Transform)find_function(
        "_ZNK20CoordinateTransforms23physicsToWorldTransformEff");
    FloatBits result[2];
    transform(result, NULL, damage_lua.to_number(lua, 1), damage_lua.to_number(lua, 2));
    damage_lua.push_number(lua, result[0].number);
    damage_lua.push_number(lua, result[1].number);
    return 2;
}

static int tf_create(void *lua) {
    if (tf_count >= 8 || damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 1) != 5)
        fail("Unexpected trajectory renderer boundary\n");
    uint32_t *event = tf_events[tf_count];
    event[0] = ++tf_serial;
    event[1] = (uint32_t)damage_lua.to_number(lua, 3);
    for (uint32_t index = 0; index < 5; ++index)
        if (damage_named(lua, 2, tf_sprites[index]))
            event[2] = index + 1;
    if (!event[2])
        fail("Unknown original trajectory sprite\n");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.set_field(lua, 1, "luaAssetRenderer");
    tf_push_value(lua, 1);
    damage_lua.set_field(lua, -10002, tf_records[tf_count++]);
    damage_lua.push_number(lua, (float)tf_serial);
    return 1;
}

static uint32_t tf_object_role(void *lua, int index) {
    const void *pointer = damage_lua.to_pointer(lua, index);
    for (uint32_t role = 0; role < 3; ++role)
        if (pointer == tf_objects[role])
            return role + 1;
    fail("Foreign original trajectory object\n");
    return 0;
}

static uint32_t tf_numeric_field(void *lua, int table, const char *name) {
    damage_lua.get_field(lua, table, name);
    FloatBits result = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, -2);
    return result.bits;
}

static uint32_t tf_snapshot(void *lua, uint32_t output[56]) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "TrajectoryRenderer");
    damage_lua.get_field(lua, 1, "addObjectToFollow");
    /* This stripped prototype has no debug upvalue names, so the original
     * lua_getupvalue refuses it. Read the pinned ARM closure/upvalue ABI and
     * copy its retained table TValue to an ordinary reserved stack slot. No
     * instruction, prototype, closure or upvalue is changed. */
    const uint32_t *closure = damage_lua.to_pointer(lua, 2);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || header[6] || header[7] != 2)
        fail("Unexpected original trajectory closure\n");
    const uint32_t *upvalue = (const uint32_t *)(uintptr_t)closure[6];
    const uint32_t *value = upvalue ? (const uint32_t *)(uintptr_t)upvalue[2] : NULL;
    if (!value || value[1] != 5)
        fail("Missing original trajectory state table\n");
    damage_lua.push_nil(lua);
    uint32_t *top = (uint32_t *)(uintptr_t)((uint32_t *)lua)[2];
    top[-2] = value[0];
    top[-1] = value[1];
    damage_lua.get_field(lua, 3, "followedObjects");
    uint32_t count = tf_length(lua, 4);
    if (count > TRAJECTORY_CAPACITY)
        fail("Original trajectory fixture capacity exceeded\n");
    for (uint32_t index = 0; index < count; ++index) {
        tf_get_index(lua, 4, (int)index + 1);
        uint32_t *row = output + index * 7;
        damage_lua.get_field(lua, 5, "object");
        row[0] = tf_object_role(lua, 6);
        damage_lua.set_top(lua, 5);
        damage_lua.get_field(lua, 5, "lastTrajectoryPosition");
        row[1] = tf_numeric_field(lua, 6, "x");
        row[2] = tf_numeric_field(lua, 6, "y");
        damage_lua.set_top(lua, 5);
        damage_lua.get_field(lua, 5, "latestSpriteId");
        row[3] = (uint32_t)damage_lua.to_number(lua, 6);
        damage_lua.set_top(lua, 5);
        damage_lua.get_field(lua, 5, "sprites");
        row[4] = tf_length(lua, 6);
        damage_lua.set_top(lua, 5);
        damage_lua.get_field(lua, 5, "inWorldCoordinates");
        row[5] = damage_lua.to_boolean(lua, 6) != 0;
        damage_lua.set_top(lua, 5);
        damage_lua.get_field(lua, 5, "velocityBasedAngle");
        row[6] = damage_lua.to_boolean(lua, 6) != 0;
        damage_lua.set_top(lua, 4);
    }
    damage_lua.set_top(lua, 0);
    return count;
}

static void tf_finish_events(void *lua) {
    for (uint32_t index = 0; index < tf_count; ++index) {
        damage_lua.get_field(lua, -10002, tf_records[index]);
        damage_lua.get_field(lua, 1, "attachedObject");
        tf_events[index][6] = tf_object_role(lua, 2);
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "luaAssetRenderer");
        tf_events[index][5] = tf_numeric_field(lua, 2, "angle");
        damage_lua.get_field(lua, 2, "position");
        tf_events[index][3] = tf_numeric_field(lua, 3, "x");
        tf_events[index][4] = tf_numeric_field(lua, 3, "y");
        damage_lua.set_top(lua, 0);
    }
}

static uint32_t tf_execute(void *lua, const uint32_t input[12]) {
    damage_lua.set_top(lua, 0);
    for (uint32_t role = 0; role < 3; ++role) {
        damage_lua.get_field(lua, -10002, tf_names[role]);
        damage_number(lua, 1, "x", input[4 + role * 2]);
        damage_number(lua, 1, "y", input[5 + role * 2]);
        damage_lua.set_top(lua, 0);
    }
    if (input[10])
        damage_function(lua, -10002, "createLuaAssetRenderer", tf_create);
    else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "createLuaAssetRenderer");
    }
    damage_lua.get_field(lua, -10002, "TrajectoryRenderer");
    const char *methods[] = {"addObjectToFollow", "removeObjectToFollow", "update"};
    damage_lua.get_field(lua, 1, methods[input[0]]);
    damage_lua.get_field(lua, -10002, tf_names[input[1] - 1]);
    int arguments = 1;
    if (!input[0]) {
        if (!input[3])
            damage_lua.push_nil(lua);
        else {
            uint32_t count = input[3] == 1 ? 2 : 1;
            damage_lua.create_table(lua, (int)count, 0);
            for (uint32_t index = 0; index < count; ++index) {
                damage_lua.push_string(lua, tf_sprites[3 + index]);
                void (*set_index)(void *, int, int) = (void (*)(
                    void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
                set_index(lua, 4, (int)index + 1);
            }
        }
        damage_lua.push_boolean(lua, (input[2] & 1u) != 0);
        damage_lua.push_boolean(lua, (input[2] & 2u) != 0);
        arguments = 4;
    } else if (input[0] == 2) {
        damage_lua.set_top(lua, 2);
        damage_lua.push_number(lua, 0.016f);
    }
    uint32_t failed = damage_lua.call(lua, arguments, 0, 0) != 0;
    damage_lua.set_top(lua, 0);
    return failed;
}

static void probe_trajectory_follow(void) {
    initialize_damage_lua();
    tf_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    tf_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    tf_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original trajectory state\n");
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    const char *const names[] = {
        "vectorCreate", "vectorSubtract",           "vectorDistance", "vectorLength",
        "vLength",      "vectorAngleBetweenSigned", "deepCopy"};
    const char *const files[] = {
        "/data/local/tmp/ab520/camera/trajectory-vector-create.lua",
        "/data/local/tmp/ab520/camera/touch-utils-vectorSubtract-oracle.lua",
        "/data/local/tmp/ab520/camera/touch-utils-vectorDistance-oracle.lua",
        "/data/local/tmp/ab520/camera/touch-utils-vectorLength-oracle.lua",
        "/data/local/tmp/ab520/camera/touch-utils-vLength-oracle.lua",
        "/data/local/tmp/ab520/camera/trajectory-vector-angle.lua",
        "/data/local/tmp/ab520/camera/trajectory-deep-copy.lua"};
    for (uint32_t index = 0; index < 7; ++index)
        tf_load(lua, files[index], names[index]);
    damage_function(lua, -10002, "physicsToWorldTransform", tf_transform);
    semihost(4, "AB_ANDROID_TRAJECTORY_FOLLOW_V1 8192\n");
    for (uint32_t scene = 0; scene < TRAJECTORY_SCENES; ++scene) {
        load_damage_chunk(lua, "/data/local/tmp/ab520/camera/trajectory-renderer.lua");
        tf_serial = 0;
        for (uint32_t role = 0; role < 3; ++role) {
            damage_lua.create_table(lua, 0, 2);
            tf_objects[role] = damage_lua.to_pointer(lua, 1);
            damage_lua.set_field(lua, -10002, tf_names[role]);
        }
        uint32_t count = 0;
        for (uint32_t step = 0; step < TRAJECTORY_STEPS; ++step) {
            uint32_t input[12], output[3], tracks[56] = {0};
            trajectory_command(scene, step, count, input);
            tf_count = 0;
            for (uint32_t event = 0; event < 8; ++event)
                for (uint32_t word = 0; word < 8; ++word)
                    tf_events[event][word] = 0;
            output[0] = tf_execute(lua, input);
            output[1] = count = tf_snapshot(lua, tracks);
            output[2] = tf_count;
            tf_finish_events(lua);
            print_words(input, 12);
            print_words(output, 3);
            print_words(tracks, count * 7);
            for (uint32_t event = 0; event < tf_count; ++event)
                print_words(tf_events[event], 8);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB trajectory follow reference complete\n");
}

#endif
