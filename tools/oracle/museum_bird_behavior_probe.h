#ifndef AB_ORACLE_MUSEUM_BIRD_BEHAVIOR_H
#define AB_ORACLE_MUSEUM_BIRD_BEHAVIOR_H

#include "../../tests/museum_bird_behavior_cases.h"

static uint32_t mb_input[16], mb_events[4][8], mb_count, mb_mutated;
static const void *mb_pointers[8];
static void (*mb_push_value)(void *, int);
static void (*mb_get_index)(void *, int, int);
static unsigned (*mb_length)(void *, int);
static const char *const mb_strings[] = {"",
                                         "RedBird",
                                         "Other",
                                         "BOOMERANG",
                                         "MuseumBirdTrajectoryRenderer",
                                         "MuseumBirdCameraFollowBehavior"};
static const char *const mb_tables[] = {
    "_ab_mb_physical1", "_ab_mb_physical2", "_ab_mb_script1", "_ab_mb_script2",
    "_ab_mb_other",     "_ab_mb_launch1",   "_ab_mb_launch2", "_ab_mb_marker"};

static void mb_load_function(void *lua, const char *path, const char *global) {
    char bytes[1024];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, bytes, sizeof(bytes));
    if (size <= 0 || size == sizeof(bytes) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, bytes, (unsigned)size, "original-museum-behavior"))
        fail("Cannot load complete original Museum helper\n");
    damage_lua.set_field(lua, -10002, global);
}

static MuseumCaseValue mb_encode(void *lua, int index) {
    uint32_t tag = (uint32_t)damage_lua.type(lua, index);
    if (tag == 0)
        return (MuseumCaseValue){0, 0};
    if (tag == 1)
        return (MuseumCaseValue){1, damage_lua.to_boolean(lua, index) != 0};
    if (tag == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        return (MuseumCaseValue){3, value.bits};
    }
    if (tag == 4) {
        for (uint32_t value = 0; value < 6; ++value)
            if (damage_named(lua, index, mb_strings[value]))
                return (MuseumCaseValue){4, value};
    }
    if (tag == 5) {
        const void *pointer = damage_lua.to_pointer(lua, index);
        for (uint32_t value = 0; value < 8; ++value)
            if (pointer == mb_pointers[value])
                return (MuseumCaseValue){5, value + 1u};
    }
    fail("Unexpected selected Museum behavior value\n");
    return (MuseumCaseValue){0, 0};
}

static void mb_push(void *lua, MuseumCaseValue value) {
    if (value.tag == 0)
        damage_lua.push_nil(lua);
    else if (value.tag == 1)
        damage_lua.push_boolean(lua, value.value != 0);
    else if (value.tag == 3) {
        FloatBits number = {.bits = value.value};
        damage_lua.push_number(lua, number.number);
    } else if (value.tag == 4) {
        if (value.value >= 6)
            fail("Unknown Museum behavior text role\n");
        damage_lua.push_string(lua, mb_strings[value.value]);
    } else if (value.tag == 5) {
        if (!value.value || value.value > 8)
            fail("Unknown Museum behavior table role\n");
        damage_lua.get_field(lua, -10002, mb_tables[value.value - 1u]);
    } else
        fail("Unknown Museum behavior scalar tag\n");
}

static void mb_mutate(void *lua) {
    if (mb_mutated)
        return;
    mb_mutated = 1;
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_mb_instance");
    int instance = top + 1;
    if (mb_input[14] & 1u) {
        mb_push(lua, (MuseumCaseValue){5, 2});
        damage_lua.set_field(lua, instance, "physicsObject");
    }
    if (mb_input[14] & 2u) {
        mb_push(lua, (MuseumCaseValue){5, 2});
        damage_lua.set_field(lua, -10002, "flyingBird");
    }
    if (mb_input[14] & 4u) {
        damage_lua.get_field(lua, -10002, "TrajectoryRenderer");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -2, "removeUnattachedRenderers");
        damage_lua.set_top(lua, instance);
    }
    if (mb_input[14] & 8u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, instance, "flying");
    }
    if (mb_input[14] & 16u) {
        mb_push(lua, (MuseumCaseValue){5, 4});
        damage_lua.set_field(lua, instance, "gameObject");
    }
    if (mb_input[14] & 32u) {
        for (uint32_t script = 2; script < 4; ++script) {
            damage_lua.get_field(lua, -10002, mb_tables[script]);
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -2, "get");
            damage_lua.set_top(lua, instance);
        }
    }
    damage_lua.set_top(lua, top);
}

static uint32_t *mb_event(uint32_t kind) {
    if (mb_count >= 4)
        fail("Museum behavior service trace exceeds bound\n");
    uint32_t *event = mb_events[mb_count++];
    event[0] = kind;
    event[7] = mb_count;
    return event;
}

static int mb_trajectory_add(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected Museum trajectory add arguments\n");
    MuseumCaseValue object = mb_encode(lua, 1);
    uint32_t *event = mb_event(1);
    event[1] = object.tag;
    event[2] = object.value;
    mb_mutate(lua);
    return 0;
}

static int mb_trajectory_remove(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected Museum trajectory removal arguments\n");
    MuseumCaseValue object = mb_encode(lua, 1);
    uint32_t *event = mb_event(2);
    event[1] = object.tag;
    event[2] = object.value;
    mb_mutate(lua);
    return 0;
}

static int mb_trajectory_keep(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5)
        fail("Unexpected Museum trajectory keep list\n");
    mb_get_index(lua, 1, 1);
    MuseumCaseValue object = mb_encode(lua, 2);
    if (mb_length(lua, 1) != (object.tag ? 1u : 0u))
        fail("Unexpected original Museum keep-list length\n");
    uint32_t *event = mb_event(3);
    event[1] = object.tag;
    event[2] = object.value;
    mb_mutate(lua);
    return 0;
}

static int mb_camera(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5)
        fail("Unexpected Museum camera service receiver\n");
    uint32_t sequence = damage_named(lua, 2, "FollowShotBirdCameraSequence") ? 1u
                        : damage_named(lua, 2, "FollowBoomerangBird")        ? 2u
                                                                             : 0u;
    if (!sequence)
        fail("Unknown original Museum camera sequence request\n");
    uint32_t *event = mb_event(4);
    event[5] = sequence;
    mb_mutate(lua);
    return 0;
}

static int mb_property(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected Museum getter arguments\n");
    MuseumCaseValue receiver = mb_encode(lua, 1);
    if (receiver.tag != 5 || (receiver.value != 3 && receiver.value != 4))
        fail("Unexpected original Museum getter receiver\n");
    uint32_t key = damage_named(lua, 2, "specialty")        ? 1u
                   : damage_named(lua, 2, "launchVelocity") ? 2u
                                                            : 0u;
    if (!key)
        fail("Unexpected original Museum property key\n");
    MuseumCaseValue value = key == 1 ? museum_specialty(mb_input, receiver.value)
                                     : museum_launch(mb_input, receiver.value);
    uint32_t *event = mb_event(5);
    event[1] = receiver.tag;
    event[2] = receiver.value;
    event[3] = value.tag;
    event[4] = value.value;
    event[6] = key;
    mb_mutate(lua);
    mb_push(lua, value);
    return 1;
}

static void mb_fields(void *lua, uint32_t output[8]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_mb_instance");
    const char *const names[] = {"name", "gameObject", "physicsObject", "flying"};
    for (uint32_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, top + 1, names[index]);
        MuseumCaseValue value = mb_encode(lua, top + 2);
        output[index * 2] = value.tag;
        output[index * 2 + 1] = value.value;
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
}

static void mb_binding(void *lua, int table, const char *name, int (*function)(void *),
                       uint32_t mask) {
    if (mb_input[15] & mask)
        damage_lua.push_closure(lua, function, 0);
    else
        damage_lua.push_nil(lua);
    damage_lua.set_field(lua, table, name);
}

static void mb_prepare(void *lua, uint32_t initial[8]) {
    damage_lua.set_top(lua, 0);
    for (uint32_t role = 0; role < 8; ++role) {
        damage_lua.create_table(lua, 0, 4);
        mb_pointers[role] = damage_lua.to_pointer(lua, 1);
        damage_lua.set_field(lua, -10002, mb_tables[role]);
    }
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, mb_tables[index]);
        mb_push(lua, museum_definition(mb_input, index + 1u));
        damage_lua.set_field(lua, 1, "definition");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, mb_tables[index + 2]);
        mb_push(lua, index ? (MuseumCaseValue){5, 2}
                           : museum_scalar(mb_input[3], mb_input[8], 1));
        damage_lua.set_field(lua, 1, "physicsObject");
        mb_binding(lua, 1, "get", mb_property, 16);
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, mb_tables[index + 5]);
        mb_push(lua, museum_launch_x(mb_input, index + 6u));
        damage_lua.set_field(lua, 1, "x");
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, mb_tables[4]);
    mb_push(lua, museum_scalar(mb_input[4], mb_input[8], 8));
    damage_lua.set_field(lua, 1, "controllable");
    mb_push(lua, museum_scalar(mb_input[5], mb_input[8], 8));
    damage_lua.set_field(lua, 1, "name");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "TrajectoryRenderer");
    mb_binding(lua, 1, "addObjectToFollow", mb_trajectory_add, 1);
    mb_binding(lua, 1, "removeObjectToFollow", mb_trajectory_remove, 2);
    mb_binding(lua, 1, "removeUnattachedRenderers", mb_trajectory_keep, 4);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    mb_binding(lua, 1, "startCameraSequenceFromFile", mb_camera, 8);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_mb_create");
    damage_lua.push_string(lua, mb_strings[mb_input[1] ? 5 : 4]);
    mb_push(lua, (MuseumCaseValue){5, 3});
    if (mb_input[0] % 3u == 0)
        damage_lua.push_nil(lua);
    else if (mb_input[0] % 3u == 1)
        damage_lua.push_boolean(lua, 0);
    else
        damage_lua.create_table(lua, 0, 0);
    if (damage_lua.call(lua, 3, 1, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original Museum constructor failed\n");
    }
    if (damage_lua.type(lua, 1) != 5)
        fail("Original Museum constructor did not return an instance\n");
    damage_lua.set_field(lua, -10002, "_ab_mb_instance");
    mb_fields(lua, initial);
    damage_lua.get_field(lua, -10002, "_ab_mb_instance");
    mb_push(lua, museum_scalar(mb_input[7], mb_input[8], 8));
    damage_lua.set_field(lua, 1, "flying");
    damage_lua.set_top(lua, 0);
    const MuseumCaseValue flying[] = {
        {0, 0}, museum_scalar(mb_input[3], mb_input[8], 1),
        {5, 2}, {1, 0},
        {1, 1}, {3, mb_input[8]}};
    mb_push(lua, flying[mb_input[6]]);
    damage_lua.set_field(lua, -10002, "flyingBird");
}

static void *mb_open_reference(void) {
    initialize_damage_lua();
    mb_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    mb_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    mb_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original Museum behavior state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "lua");
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/behavior-class.lua");
    mb_load_function(lua, "/data/local/tmp/ab520/camera/behavior-shallow-copy.lua",
                     "shallowCopy");
    mb_load_function(lua, "/data/local/tmp/ab520/camera/behavior-merge-recursive.lua",
                     "mergeTablesRecursive");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.set_field(lua, -10002, "TrajectoryRenderer");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/behavior-base.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/museum-bird-trajectory.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/museum-camera-full.lua");
    mb_load_function(lua, "/data/local/tmp/ab520/camera/behavior-utils-create.lua",
                     "_ab_mb_create");
    damage_lua.create_table(lua, 0, 2);
    for (uint32_t kind = 0; kind < 2; ++kind) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, mb_strings[kind ? 5 : 4]);
        damage_lua.set_field(lua, 2, "class");
        damage_lua.set_field(lua, 1, mb_strings[kind ? 5 : 4]);
    }
    damage_lua.set_field(lua, -10002, "BehaviorList");
    return lua;
}

static void probe_museum_bird_behavior(void) {
    void *lua = mb_open_reference();
    semihost(4, "AB_ANDROID_MUSEUM_BIRD_BEHAVIOR_V1 2048\n");
    const char *const methods[] = {"onLaunch", "onBirdCollision", "onDestroy",
                                   "onTriggerSpecialty"};
    for (uint32_t sample = 0; sample < MUSEUM_BEHAVIOR_CASES; ++sample) {
        museum_behavior_case(sample, mb_input);
        mb_count = mb_mutated = 0;
        for (uint32_t event = 0; event < 4; ++event)
            for (uint32_t word = 0; word < 8; ++word)
                mb_events[event][word] = 0;
        uint32_t output[20] = {0};
        mb_prepare(lua, output + 2);
        damage_lua.get_field(lua, -10002, "_ab_mb_instance");
        damage_lua.get_field(lua, 1, methods[mb_input[2]]);
        if (damage_lua.type(lua, 2) == 6) {
            mb_push_value(lua, 1);
            if (mb_input[2] == 1) {
                mb_push(lua, (MuseumCaseValue){5, 1});
                mb_push(lua, (MuseumCaseValue){5, 5});
            }
            int arguments = mb_input[2] == 1 ? 3 : 1;
            output[0] = damage_lua.call(lua, arguments, 0, 0) != 0;
        } else if (damage_lua.type(lua, 2) != 0)
            fail("Unexpected original Museum event binding\n");
        damage_lua.set_top(lua, 0);
        output[1] = mb_count;
        mb_fields(lua, output + 10);
        output[18] = mb_mutated;
        print_words(mb_input, 16);
        print_words(output, 20);
        for (uint32_t event = 0; event < mb_count; ++event)
            print_words(mb_events[event], 8);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB museum bird behavior reference complete\n");
}

#endif
