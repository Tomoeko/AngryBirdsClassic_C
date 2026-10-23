#ifndef AB_ORACLE_MIGHTY_EAGLE_TIMER_H
#define AB_ORACLE_MIGHTY_EAGLE_TIMER_H

#include "../../tests/mighty_eagle_timer_cases.h"

static uint32_t mighty_timer_input[6], mighty_timer_events[5][3], mighty_timer_count;
static const char *const mighty_timer_fields[] = {
    "eagleTimer",       "eagleMoving", "eagleX",        "eagleY",
    "eagleSoundPlayed", "cameraShake", "destroyJoints", "birdSpecialtyAvailable"};

static uint32_t *mighty_timer_event(uint32_t operation) {
    if (mighty_timer_count == 5)
        fail("Original Eagle timer exceeds controlled event storage\n");
    uint32_t *event = mighty_timer_events[mighty_timer_count++];
    event[0] = operation;
    return event;
}

static void mighty_timer_number(void *lua, const char *name, float number) {
    damage_lua.push_number(lua, number);
    damage_lua.set_field(lua, -10002, name);
}

static void mighty_timer_mutate(void *lua, int create) {
    if (!(mighty_timer_input[5] & 256))
        return;
    mighty_timer_number(lua, "cameraShake", 50);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "destroyJoints");
    if (create)
        mighty_timer_number(lua, "eagleTimer", -0.25f);
}

static int mighty_timer_create(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected original Eagle construction arity\n");
    uint32_t *event = mighty_timer_event(1);
    for (int index = 1; index <= 2; ++index) {
        if (damage_lua.type(lua, index) != 3)
            fail("Original Eagle construction requires numeric coordinates\n");
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        event[index] = value.bits;
    }
    mighty_timer_mutate(lua, 1);
    return 0;
}

static int mighty_timer_audio(void *lua) {
    if (damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 2) != 3 ||
        damage_lua.to_number(lua, 2) != 1 || damage_lua.type(lua, 3) != 1 ||
        damage_lua.to_boolean(lua, 3))
        fail("Unexpected original Eagle timer audio arguments\n");
    int yell = damage_named(lua, 1, "mighty_eagle_yell");
    if (!yell && !damage_named(lua, 1, "mighty_eagle_fly"))
        fail("Unexpected original Eagle audio identifier\n");
    (void)mighty_timer_event(yell ? 2u : 3u);
    if (yell)
        mighty_timer_mutate(lua, 0);
    return 0;
}

/* Observe the source world/joint lookup at each cleanup phase. The controlled
 * world is empty and the joints table absent; concrete destructive loops are
 * outside this timer boundary corpus. */
static int mighty_timer_objects(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected original Eagle objects lookup\n");
    if (damage_named(lua, 2, "world")) {
        (void)mighty_timer_event(4);
        damage_lua.create_table(lua, 0, 0);
    } else if (damage_named(lua, 2, "joints")) {
        (void)mighty_timer_event(5);
        damage_lua.push_nil(lua);
    } else
        fail("Unexpected original Eagle timer object field\n");
    return 1;
}

static void mighty_timer_prepare(void *lua) {
    uint32_t flags = mighty_timer_input[5];
    for (size_t field = 0; field < 8; ++field) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, mighty_timer_fields[field]);
    }
    if (flags & 1) {
        FloatBits value = {.bits = mighty_timer_input[0]};
        mighty_timer_number(lua, "eagleTimer", value.number);
    }
    damage_lua.push_boolean(lua, (flags & 2) != 0);
    damage_lua.set_field(lua, -10002, "eagleMoving");
    damage_lua.push_boolean(lua, (flags & 4) != 0);
    damage_lua.set_field(lua, -10002, "eagleSoundPlayed");
    const uint32_t masks[] = {8, 16, 32};
    const size_t fields[] = {2, 3, 5}, inputs[] = {3, 4, 2};
    for (size_t index = 0; index < 3; ++index)
        if (flags & masks[index]) {
            FloatBits value = {.bits = mighty_timer_input[inputs[index]]};
            mighty_timer_number(lua, mighty_timer_fields[fields[index]], value.number);
        }
    if (flags & 128)
        mighty_timer_number(lua, "destroyJoints", 0);
    else if (flags & 64) {
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, -10002, "destroyJoints");
    }
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "birdSpecialtyAvailable");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "__index", mighty_timer_objects);
    int (*set_meta)(void *, int) =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    if (!set_meta(lua, 1))
        fail("Cannot bind original Eagle timer observation table\n");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.set_top(lua, 0);
    mighty_timer_count = 0;
    for (size_t event = 0; event < 5; ++event)
        for (size_t field = 0; field < 3; ++field)
            mighty_timer_events[event][field] = 0;
}

static void probe_mighty_eagle_timer(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original Mighty Eagle timer state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/mighty-eagle.lua");
    damage_function(lua, -10002, "createMightyEagle", mighty_timer_create);
    damage_function(lua, -10002, "playAudio", mighty_timer_audio);
    semihost(4, "AB_ANDROID_MIGHTY_EAGLE_TIMER_V1 1024\n");
    for (uint32_t sample = 0; sample < AB_MIGHTY_EAGLE_TIMER_CASES; ++sample) {
        mighty_timer_case(sample, mighty_timer_input);
        mighty_timer_prepare(lua);
        damage_lua.get_field(lua, -10002, "mightyEagleAndBaitTimerUpdate");
        FloatBits seconds = {.bits = mighty_timer_input[1]};
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 1, 0, 0))
            fail("Original Mighty Eagle timer invocation failed\n");
        uint32_t output[16] = {0};
        for (size_t field = 0; field < 8; ++field) {
            damage_lua.get_field(lua, -10002, mighty_timer_fields[field]);
            uint32_t type = (uint32_t)damage_lua.type(lua, 1);
            output[field * 2] = type;
            if (type == 3) {
                FloatBits value = {.number = damage_lua.to_number(lua, 1)};
                output[field * 2 + 1] = value.bits;
            } else if (type == 1)
                output[field * 2 + 1] = (uint32_t)damage_lua.to_boolean(lua, 1);
            else if (type != 0)
                fail("Unexpected original Eagle timer output type\n");
            damage_lua.set_top(lua, 0);
        }
        print_words(mighty_timer_input, 6);
        print_words(output, 16);
        print_words(&mighty_timer_count, 1);
        for (size_t event = 0; event < mighty_timer_count; ++event)
            print_words(mighty_timer_events[event], 3);
    }
    semihost(4, "AB mighty eagle timer reference complete\n");
    damage_lua.close_state(lua);
}

#endif
