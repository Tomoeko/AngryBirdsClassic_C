#ifndef AB_ORACLE_SLING_LAUNCH_HOST_H
#define AB_ORACLE_SLING_LAUNCH_HOST_H

#include "../../tests/sling_launch_cases.h"

static SlingLaunchFixture launch_fixture;
static uint32_t launch_input[20], launch_events[9][21], launch_event_count;
static uint32_t launch_release_events[2], launch_release_count, launch_selected;
static float launch_last_impulse[2];
static const char *const launch_birds[] = {"_ab_launch_bird_A", "_ab_launch_bird_B"};

static uint32_t launch_numeric(void *lua, int index, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, index, name);
    if (damage_lua.type(lua, -1) != 3)
        fail("Expected numeric original launch fixture field\n");
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static const void *launch_pointer(void *lua, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    const void *pointer = damage_lua.to_pointer(lua, -1);
    damage_lua.set_top(lua, top);
    return pointer;
}

static bool launch_predicate(void *lua, const char *name, bool literal_true) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    bool value = damage_lua.to_boolean(lua, -1) != 0;
    if (literal_true)
        value = value && damage_lua.type(lua, -1) == 1;
    damage_lua.set_top(lua, top);
    return value;
}

static void launch_tagged(void *lua, uint32_t tag, int index, const char *name) {
    if (tag == 0)
        damage_lua.push_nil(lua);
    else if (tag == 1 || tag == 2)
        damage_lua.push_boolean(lua, tag == 2);
    else if (tag == 3)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_string(lua, "mode");
    damage_lua.set_field(lua, index, name);
}

static void launch_set_selected(void *lua, uint32_t role) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, -1, "setDraggedBird");
    damage_lua.get_field(lua, -10002, "_ab_launch_self");
    damage_lua.get_field(lua, -10002, launch_birds[role]);
    if (damage_lua.call(lua, 2, 0, 0))
        fail("Cannot bind original dragged-bird state through its setter\n");
    damage_lua.set_top(lua, top);
    launch_selected = role;
}

static uint32_t launch_read_selected(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, -1, "getDraggedBird");
    damage_lua.get_field(lua, -10002, "_ab_launch_self");
    if (damage_lua.call(lua, 1, 1, 0))
        fail("Cannot observe original dragged-bird getter\n");
    const void *pointer = damage_lua.to_pointer(lua, -1);
    uint32_t role = 2;
    for (uint32_t index = 0; index < 2; ++index)
        if (pointer == launch_pointer(lua, launch_birds[index]))
            role = index;
    damage_lua.set_top(lua, top);
    if (role == 2)
        fail("Unknown original dragged-bird identity\n");
    return role;
}

static void launch_read_state(void *lua) {
    int top = damage_lua.get_top(lua);
    launch_fixture.role = launch_read_selected(lua);
    launch_fixture.eagle = launch_predicate(lua, "eagleBaitLaunched", true);
    launch_fixture.special = launch_predicate(lua, "g_special_game_mode", false);
    launch_fixture.default_force =
        sling_launch_number(launch_numeric(lua, -10002, "defaultForce"));
    launch_fixture.scale =
        sling_launch_number(launch_numeric(lua, -10002, "physicsScale"));
    launch_fixture.shots =
        sling_launch_number(launch_numeric(lua, -10002, "_ab_launch_shots"));
    launch_fixture.backwards =
        sling_launch_number(launch_numeric(lua, -10002, "_ab_launch_backwards"));
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, launch_birds[index]);
        launch_fixture.mass[index] =
            sling_launch_number(launch_numeric(lua, -1, "mass"));
        damage_lua.set_top(lua, top);
    }
    const char *vectors[] = {"_ab_launch_direction", "_ab_launch_band"};
    float *values[] = {launch_fixture.direction, launch_fixture.band};
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, vectors[index]);
        values[index][0] = sling_launch_number(launch_numeric(lua, -1, "x"));
        values[index][1] = sling_launch_number(launch_numeric(lua, -1, "y"));
        damage_lua.set_top(lua, top);
    }
}

static void launch_write_state(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_number(lua, -10002, "defaultForce",
                  sling_launch_word(launch_fixture.default_force));
    damage_number(lua, -10002, "physicsScale", sling_launch_word(launch_fixture.scale));
    damage_number(lua, -10002, "_ab_launch_shots",
                  sling_launch_word(launch_fixture.shots));
    damage_number(lua, -10002, "_ab_launch_backwards",
                  sling_launch_word(launch_fixture.backwards));
    if (launch_predicate(lua, "eagleBaitLaunched", true) != launch_fixture.eagle) {
        damage_lua.push_boolean(lua, launch_fixture.eagle);
        damage_lua.set_field(lua, -10002, "eagleBaitLaunched");
    }
    if (launch_predicate(lua, "g_special_game_mode", false) != launch_fixture.special) {
        damage_lua.push_boolean(lua, launch_fixture.special);
        damage_lua.set_field(lua, -10002, "g_special_game_mode");
    }
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, launch_birds[index]);
        damage_number(lua, -2, "mass", sling_launch_word(launch_fixture.mass[index]));
        damage_lua.set_top(lua, top);
    }
    const char *vectors[] = {"_ab_launch_direction", "_ab_launch_band"};
    float *values[] = {launch_fixture.direction, sling_launch_band(&launch_fixture)};
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, vectors[index]);
        damage_number(lua, -2, "x", sling_launch_word(values[index][0]));
        damage_number(lua, -2, "y", sling_launch_word(values[index][1]));
        damage_lua.set_top(lua, top);
    }
    if (launch_selected != launch_fixture.role)
        launch_set_selected(lua, launch_fixture.role);
}

static int launch_service(void *lua) {
    uint32_t operation = (uint32_t)damage_lua.to_number(lua, -10003);
    if (operation < 1 || operation > 9 || launch_event_count == 9)
        fail("Unexpected original launch service\n");
    launch_read_state(lua);
    uint32_t *event = launch_events[launch_event_count++];
    event[0] = operation;
    if (operation == 6) {
        event[1] = damage_named(lua, 1, "bird_A")   ? 1u
                   : damage_named(lua, 1, "bird_B") ? 2u
                                                    : 0u;
        if (!event[1])
            fail("Original force query used an unknown bird name\n");
    }
    float impulse[2] = {0};
    if (operation == 9) {
        if (damage_lua.get_top(lua) != 3 ||
            damage_lua.to_pointer(lua, 2) != launch_pointer(lua, "_ab_launch_band"))
            fail("Original launch position alias differs from getter result\n");
        event[2] = launch_numeric(lua, 2, "x");
        event[3] = launch_numeric(lua, 2, "y");
        event[4] = launch_numeric(lua, 3, "x");
        event[5] = launch_numeric(lua, 3, "y");
        const void *direction = launch_pointer(lua, "_ab_launch_direction");
        event[19] = damage_lua.to_pointer(lua, 2) == direction;
        event[20] = damage_lua.to_pointer(lua, 3) != direction &&
                    damage_lua.to_pointer(lua, 3) != damage_lua.to_pointer(lua, 2);
        impulse[0] = sling_launch_number(event[4]);
        impulse[1] = sling_launch_number(event[5]);
    }
    sling_launch_snapshot(&launch_fixture, event + 6);
    sling_launch_effect(&launch_fixture, operation, impulse);
    launch_write_state(lua);
    if (operation == 1 || operation == 8) {
        damage_lua.get_field(
            lua, -10002, operation == 1 ? "_ab_launch_direction" : "_ab_launch_band");
        return 1;
    }
    if (operation == 2 || operation == 5 || operation == 7) {
        uint32_t field = operation == 2 ? 2 : operation == 5 ? 7 : 8;
        damage_lua.push_number(lua, sling_launch_number(launch_input[field]));
        return 1;
    }
    if (operation == 6) {
        damage_lua.create_table(lua, 0, 1);
        if (launch_input[17] == 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, -2, "slingshotLaunchForce");
        } else if (launch_input[17] == 2) {
            damage_number(lua, -2, "slingshotLaunchForce", launch_input[9]);
        }
        return 1;
    }
    if (operation == 9) {
        damage_number(lua, 3, "x", sling_launch_word(impulse[0]));
        damage_number(lua, 3, "y", sling_launch_word(impulse[1]));
        launch_last_impulse[0] = impulse[0];
        launch_last_impulse[1] = impulse[1];
    }
    return 0;
}

static int launch_release_service(void *lua) {
    uint32_t operation = (uint32_t)damage_lua.to_number(lua, -10003);
    if (operation < 1 || operation > 3 || launch_release_count == 2)
        fail("Unexpected original release service\n");
    launch_release_events[launch_release_count++] = operation;
    if (operation != 1)
        return 0;
    uint32_t tag = launch_input[14];
    if (tag == 0)
        damage_lua.push_nil(lua);
    else if (tag == 1 || tag == 2)
        damage_lua.push_boolean(lua, tag == 2);
    else if (tag == 3)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_string(lua, "mode");
    return 1;
}

static void launch_bind(void *lua, int table, const char *name, int (*function)(void *),
                        uint32_t operation, uint32_t failure) {
    if (operation == failure) {
        damage_lua.push_nil(lua);
    } else {
        damage_lua.push_number(lua, (float)operation);
        damage_lua.push_closure(lua, function, 1);
    }
    damage_lua.set_field(lua, table, name);
}

static void launch_prepare(void *lua) {
    launch_fixture = sling_launch_fixture(launch_input);
    launch_event_count = launch_release_count = 0;
    launch_last_impulse[0] = launch_last_impulse[1] = 0;
    for (uint32_t event = 0; event < 9; ++event)
        for (uint32_t field = 0; field < 21; ++field)
            launch_events[event][field] = 0;
    launch_release_events[0] = launch_release_events[1] = 0;
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 1, "x", launch_input[0]);
    damage_number(lua, 1, "y", launch_input[1]);
    damage_lua.set_field(lua, -10002, "_ab_launch_direction");
    if (launch_fixture.alias_band) {
        damage_lua.get_field(lua, -10002, "_ab_launch_direction");
    } else {
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 1, "x", launch_input[10]);
        damage_number(lua, 1, "y", launch_input[11]);
    }
    damage_lua.set_field(lua, -10002, "_ab_launch_band");
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_string(lua, index ? "bird_B" : "bird_A");
        damage_lua.set_field(lua, 1, "name");
        damage_number(lua, 1, "mass", launch_input[5 + index]);
        damage_lua.set_field(lua, -10002, launch_birds[index]);
    }
    launch_tagged(lua, launch_input[12], -10002, "eagleBaitLaunched");
    launch_tagged(lua, launch_input[13], -10002, "g_special_game_mode");
    damage_lua.create_table(lua, 0, 6);
    const char *names[] = {"getVectorToRestPosition", "getDistanceToRestPosition",
                           "getStretchFactor",        "getSlingshotPower",
                           "getRubberBandPosition",   "launchBirdWithImpulse"};
    const uint32_t operations[] = {1, 2, 5, 7, 8, 9};
    for (uint32_t index = 0; index < 6; ++index)
        launch_bind(lua, 1, names[index], launch_service, operations[index],
                    launch_input[18]);
    damage_lua.set_field(lua, -10002, "_ab_launch_self");
    damage_lua.create_table(lua, 0, 2);
    launch_bind(lua, 1, "incrementBackwardsBirdCount", launch_service, 3,
                launch_input[18]);
    launch_bind(lua, 1, "incrementBirdsShot", launch_service, 4, launch_input[18]);
    damage_lua.set_field(lua, -10002, "settingsWrapper");
    launch_bind(lua, -10002, "getObjectDefinition", launch_service, 6,
                launch_input[18]);
    damage_lua.create_table(lua, 0, 3);
    const char *release[] = {"isStretchedEnoughToShootBird", "launchBird",
                             "cancelBirdDrag"};
    for (uint32_t index = 0; index < 3; ++index)
        launch_bind(lua, 1, release[index], launch_release_service, index + 1,
                    launch_input[19]);
    damage_lua.set_field(lua, -10002, "_ab_release_host_self");
    launch_selected = 2;
    launch_write_state(lua);
}

static void probe_sling_launch_host(void) {
    void *lua = open_sling_reference();
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "stopMovement", damage_require);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    semihost(4, "AB_ANDROID_SLING_LAUNCH_HOST_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        sling_launch_case(sample, launch_input);
        launch_prepare(lua);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "launchBird");
        damage_lua.get_field(lua, -10002, "_ab_launch_self");
        if (launch_input[15] & 1)
            damage_lua.get_field(lua, -10002, "_ab_launch_direction");
        else
            damage_lua.push_nil(lua);
        uint32_t success = damage_lua.call(lua, 2, 0, 0) == 0;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "releaseRubberBandToLaunchBird");
        damage_lua.get_field(lua, -10002, "_ab_release_host_self");
        uint32_t released = damage_lua.call(lua, 1, 0, 0) == 0;
        damage_lua.set_top(lua, 0);
        launch_read_state(lua);
        uint32_t prefix[] = {success, released, launch_event_count,
                             launch_release_count};
        print_words(launch_input, 20);
        print_words(prefix, 4);
        for (uint32_t event = 0; event < launch_event_count; ++event)
            print_words(launch_events[event], 21);
        uint32_t snapshot[13];
        sling_launch_snapshot(&launch_fixture, snapshot);
        print_words(snapshot, 13);
        uint32_t impulse[] = {sling_launch_word(launch_last_impulse[0]),
                              sling_launch_word(launch_last_impulse[1])};
        print_words(impulse, 2);
        print_words(launch_release_events, 2);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling launch host reference complete\n");
}

#endif
