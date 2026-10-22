#ifndef AB_ORACLE_BLUES_TRAJECTORY_H
#define AB_ORACLE_BLUES_TRAJECTORY_H

#include "museum_bird_behavior_probe.h"
#include "../../tests/blues_trajectory_cases.h"

static uint32_t blues_input[BLUES_INPUT_WORDS], blues_events[4][BLUES_EVENT_WORDS];
static uint32_t blues_event_count;
static BluesEnvironment blues_env;
static void (*blues_set_index)(void *, int, int);
static const char *const blues_fields[] = {"isCluster", "flying", "split",
                                           "physicsObject", "originalCluster"};

static BluesValue blues_read(void *lua, int index) {
    MuseumCaseValue value = mb_encode(lua, index);
    return (BluesValue){value.tag, value.value};
}

static void blues_push(void *lua, BluesValue value) {
    mb_push(lua, (MuseumCaseValue){value.tag, value.value});
}

static void blues_read_state(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_blues_self");
    int self = top + 1;
    for (uint32_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, self, blues_fields[field]);
        blues_env.fields[field] = blues_read(lua, -1);
        damage_lua.set_top(lua, self);
    }
    const char *globals[] = {"flyingBird", "otherBirds"};
    BluesValue *values[] = {&blues_env.flying, &blues_env.others};
    for (uint32_t field = 0; field < 2; ++field) {
        damage_lua.get_field(lua, -10002, globals[field]);
        *values[field] = blues_read(lua, -1);
        damage_lua.set_top(lua, self);
    }
    damage_lua.set_top(lua, top);
}

static void blues_write_state(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_blues_self");
    int self = top + 1;
    for (uint32_t field = 0; field < 5; ++field) {
        blues_push(lua, blues_env.fields[field]);
        damage_lua.set_field(lua, self, blues_fields[field]);
    }
    blues_push(lua, blues_env.flying);
    damage_lua.set_field(lua, -10002, "flyingBird");
    blues_push(lua, blues_env.others);
    damage_lua.set_field(lua, -10002, "otherBirds");
    damage_lua.set_top(lua, top);
}

static int blues_service(void *lua) {
    uint32_t operation = (uint32_t)damage_lua.to_number(lua, -10003);
    if (!operation || operation > 4 || blues_event_count == 4 ||
        damage_lua.get_top(lua) != 1)
        fail("Unexpected original Blues service\n");
    blues_read_state(lua);
    if (operation == 4)
        ++blues_env.get_calls;
    uint32_t *event = blues_events[blues_event_count++];
    event[0] = operation;
    if (operation == 3) {
        if (damage_lua.type(lua, 1) != 5)
            fail("Original Blues keep argument is not a table\n");
        for (int index = 1; index <= 4; ++index) {
            mb_get_index(lua, 1, index);
            BluesValue value = blues_read(lua, -1);
            event[3] += value.tag != 0;
            event[4 + (uint32_t)(index - 1) * 2] = value.tag;
            event[5 + (uint32_t)(index - 1) * 2] = value.value;
            damage_lua.set_top(lua, 1);
        }
    } else {
        BluesValue value = blues_read(lua, 1);
        event[1] = value.tag;
        event[2] = value.value;
    }
    blues_snapshot(&blues_env, event + 12);
    blues_mutate(blues_input, &blues_env);
    blues_write_state(lua);
    if (operation != 4)
        return 0;
    if (blues_env.get_calls == 1 && blues_input[13] == 2) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "getObject");
    }
    uint32_t bit = blues_env.get_calls == 1 ? 1u : 2u;
    BluesValue result = blues_input[10] & bit
                            ? (BluesValue){0, 0}
                            : blues_physical(blues_env.get_calls == 1 ? 2u : 1u);
    blues_push(lua, result);
    return 1;
}

static void blues_function(void *lua, int table_index, const char *name,
                           uint32_t operation) {
    if (blues_input[12] == operation) {
        damage_lua.push_nil(lua);
    } else {
        damage_lua.push_number(lua, (float)operation);
        damage_lua.push_closure(lua, blues_service, 1);
    }
    damage_lua.set_field(lua, table_index, name);
}

static void blues_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    for (uint32_t table_index = 0; table_index < 8; ++table_index) {
        damage_lua.create_table(lua, 2, 2);
        damage_lua.set_field(lua, -10002, mb_tables[table_index]);
    }
    for (uint32_t table_index = 0; table_index < 8; ++table_index) {
        damage_lua.get_field(lua, -10002, mb_tables[table_index]);
        mb_pointers[table_index] = damage_lua.to_pointer(lua, 1);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, mb_tables[2]);
    blues_push(lua, blues_physical(1));
    damage_lua.set_field(lua, 1, "physicsObject");
    damage_lua.set_top(lua, 0);
    for (uint32_t list = 0; list < 2; ++list) {
        damage_lua.get_field(lua, -10002, mb_tables[list + 6]);
        damage_lua.push_string(lua, list ? "Other" : "RedBird");
        blues_set_index(lua, 1, 1);
        if (list)
            damage_lua.push_nil(lua);
        else
            damage_lua.push_string(lua, "Other");
        blues_set_index(lua, 1, 2);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, "TrajectoryRenderer");
    blues_function(lua, 1, "addObjectToFollow", 1);
    blues_function(lua, 1, "removeObjectToFollow", 2);
    blues_function(lua, 1, "removeUnattachedRenderers", 3);
    damage_lua.set_top(lua, 0);
    blues_function(lua, -10002, "getObject", 4);
    damage_lua.get_field(lua, -10002, "MuseumBluesTrajectoryRenderer");
    damage_lua.get_field(lua, 1, "new");
    mb_push_value(lua, 1);
    damage_lua.push_string(lua, "MuseumBluesTrajectoryRenderer");
    damage_lua.get_field(lua, -10002, mb_tables[2]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 6, "isCluster");
    if (damage_lua.call(lua, 4, 1, 0))
        fail("Original Blues parent constructor failed\n");
    damage_lua.get_field(lua, 2, "name");
    if (!damage_named(lua, -1, "MuseumBluesTrajectoryRenderer"))
        fail("Original Blues constructor name differs\n");
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "physicsObject");
    if (damage_lua.to_pointer(lua, 3) != mb_pointers[0])
        fail("Original Blues constructor physical identity differs\n");
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "gameObject");
    if (damage_lua.to_pointer(lua, 3) != mb_pointers[2])
        fail("Original Blues constructor script identity differs\n");
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "isCluster");
    if (damage_lua.type(lua, 3) != 1 || !damage_lua.to_boolean(lua, 3))
        fail("Original Blues constructor lost its authored cluster flag\n");
    damage_lua.set_top(lua, 2);
    damage_lua.set_field(lua, -10002, "_ab_blues_self");
    damage_lua.set_top(lua, 0);
    blues_environment(blues_input, &blues_env);
    blues_write_state(lua);
    damage_lua.get_field(lua, -10002, mb_tables[4]);
    blues_push(lua, blues_predicate(blues_input[7]));
    damage_lua.set_field(lua, 1, "controllable");
    const BluesValue names[] = {{0, 0}, {1, 0}, {4, 0}, {4, 2}, {4, 1}};
    blues_push(lua, names[blues_input[8]]);
    damage_lua.set_field(lua, 1, "name");
    damage_lua.set_top(lua, 0);
}

static void probe_blues_trajectory(void) {
    (void)probe_museum_bird_behavior;
    void *lua = mb_open_reference();
    blues_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/museum-blues-trajectory.lua");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, -10002, "g_in_editor");
    semihost(4, "AB_ANDROID_BLUES_TRAJECTORY_V1 512\n");
    const char *methods[] = {"init", "onLaunch", "onTriggerSpecialty",
                             "onBirdCollision", "onDestroy"};
    for (uint32_t sample = 0; sample < BLUES_CASES; ++sample) {
        blues_case(sample, blues_input);
        blues_event_count = 0;
        for (uint32_t event = 0; event < 4; ++event)
            for (uint32_t word = 0; word < BLUES_EVENT_WORDS; ++word)
                blues_events[event][word] = 0;
        blues_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_blues_self");
        damage_lua.get_field(lua, 1, methods[blues_input[1]]);
        mb_push_value(lua, 1);
        int arguments = 1;
        if (blues_input[1] == 3) {
            blues_push(lua, blues_physical(1));
            damage_lua.get_field(lua, -10002, mb_tables[4]);
            arguments = 3;
        }
        uint32_t output[17] = {0};
        output[0] = damage_lua.call(lua, arguments, 0, 0) == 0;
        output[1] = blues_event_count;
        damage_lua.set_top(lua, 0);
        blues_read_state(lua);
        blues_snapshot(&blues_env, output + 2);
        print_words(blues_input, BLUES_INPUT_WORDS);
        print_words(output, 17);
        for (uint32_t event = 0; event < blues_event_count; ++event)
            print_words(blues_events[event], BLUES_EVENT_WORDS);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB Blues trajectory reference complete\n");
}

#endif
