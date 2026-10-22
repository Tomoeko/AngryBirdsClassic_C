#ifndef AB_ORACLE_BUBBLE_CONTACT_H
#define AB_ORACLE_BUBBLE_CONTACT_H

#include "../../tests/bubble_contact_cases.h"

typedef struct {
    uint32_t *game;
    uint32_t objects[2][50], bodies[2][38], fixtures[2][11], shapes[2][5], contact[37];
    uint32_t input[11], events[2][12], count;
    void (*reference)(void *);
} BubbleContactReference;

static BubbleContactReference bubble_contact_reference;
static const char *const bubble_contact_fields[] = {"inBubble",
                                                    "shot",
                                                    "strength",
                                                    "killSilently",
                                                    "bubbleAntiGravityTimer",
                                                    "bubbleSprite",
                                                    "bubbleSpriteScale"};

static void bubble_contact_scalar(void *lua, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, 0);
            break;
        case 2:
            damage_lua.push_boolean(lua, 1);
            break;
        case 3:
            damage_lua.push_number(lua, 0);
            break;
        case 4:
            damage_lua.push_string(lua, "seed");
            break;
        default:
            damage_lua.create_table(lua, 0, 0);
            break;
    }
}

static void bubble_contact_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        if (damage_named(lua, -1, "seed")) {
            output[1] = 1;
        } else if (damage_named(lua, -1, "BUBBLE_PARTICLE_6")) {
            output[1] = 2;
        } else {
            fail("Unexpected native bubble field string\n");
        }
    }
}

static uint32_t *bubble_contact_event(uint32_t kind) {
    BubbleContactReference *state = &bubble_contact_reference;
    if (state->count >= 2) {
        fail("Native bubble callback count overflow\n");
    }
    uint32_t *event = state->events[state->count++];
    for (size_t field = 0; field < 12; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = state->contact[1];
    event[2] = state->objects[0][40] & 255u;
    event[3] = state->objects[0][30];
    event[4] = state->objects[0][31];
    return event;
}

static int bubble_contact_trapped(void *lua) {
    BubbleContactReference *state = &bubble_contact_reference;
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Invalid native trapped-in-bubble arguments\n");
    }
    const void *argument = damage_lua.to_pointer(lua, 1);
    state->reference(state->objects[0] + 24);
    if (damage_lua.to_pointer(lua, -1) != argument) {
        fail("Native trapped-in-bubble table identity mismatch\n");
    }
    damage_lua.set_top(lua, 1);
    static const uint32_t expected[][2] = {
        {1, 1}, {3, UINT32_C(0x40a00000)}, {4, 2}, {3, UINT32_C(0x3f000000)}};
    static const char *const fields[] = {"inBubble", "bubbleAntiGravityTimer",
                                         "bubbleSprite", "bubbleSpriteScale"};
    for (size_t field = 0; field < 4; ++field) {
        uint32_t value[2];
        damage_lua.get_field(lua, 1, fields[field]);
        bubble_contact_value(lua, value);
        if (value[0] != expected[field][0] || value[1] != expected[field][1]) {
            fail("Native bubble capture fields were not set before trapped callback\n");
        }
        damage_lua.set_top(lua, 1);
    }
    state->reference((uint8_t *)state->game + 0x1828);
    damage_lua.get_field(lua, 2, "bubble");
    if (damage_lua.type(lua, -1) != 5) {
        fail("Native bubble was not marked dead before trapped callback\n");
    }
    damage_lua.set_top(lua, 1);
    uint32_t *event = bubble_contact_event(1);
    event[5] = 1;
    if (state->input[10] & 1u) {
        state->bodies[0][3] = UINT32_C(0x3e800000);
    }
    if (state->input[10] & 2u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "shot");
    }
    if (state->input[10] & 4u) {
        ((uint8_t *)state->objects[0])[0xa0] = 0;
        state->objects[0][30] = UINT32_C(0x40e00000);
        state->objects[0][31] = UINT32_C(0x41000000);
    }
    return 0;
}

static int bubble_contact_collision(void *lua) {
    if (damage_lua.get_top(lua) != 7 || !damage_named(lua, 1, "bird") ||
        !damage_named(lua, 2, "bubble") || damage_lua.type(lua, 7) != 5) {
        fail("Invalid native bubble collision arguments\n");
    }
    uint32_t *event = bubble_contact_event(2);
    event[5] = 1;
    event[6] = 2;
    for (int argument = 3; argument <= 6; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Native bubble geometry is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[argument + 4] = value.bits;
    }
    static const char *const fields[] = {
        "shouldDisableContact", "newCollisionPathParameters", "glueContactPair"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, 7, fields[field]);
        if (damage_lua.type(lua, -1) != 0) {
            fail("Native bubble response was not initially empty\n");
        }
        damage_lua.set_top(lua, 7);
    }
    event[11] = 1;
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 7, "shouldDisableContact");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 7, "newCollisionPathParameters");
    damage_lua.push_string(lua, "ignored");
    damage_lua.set_field(lua, 7, "glueContactPair");
    return 0;
}

static void bubble_contact_snapshot(void *lua) {
    BubbleContactReference *state = &bubble_contact_reference;
    uint32_t summary[] = {state->input[0],
                          state->count,
                          state->contact[1],
                          state->objects[0][40] & 255u,
                          state->objects[0][30],
                          state->objects[0][31],
                          state->bodies[0][3],
                          state->bodies[0][4],
                          0,
                          0};
    state->reference((uint8_t *)state->game + 0x1828);
    damage_lua.get_field(lua, 1, "bubble");
    summary[8] = (uint32_t)damage_lua.type(lua, -1);
    if (summary[8] == 5) {
        const void *entry = damage_lua.to_pointer(lua, -1);
        state->reference(state->objects[1] + 24);
        summary[9] = entry == damage_lua.to_pointer(lua, -1);
    }
    damage_lua.set_top(lua, 0);
    print_words(summary, 10);
    for (size_t object = 0; object < 2; ++object) {
        state->reference(state->objects[object] + 24);
        uint32_t fields[14];
        for (size_t field = 0; field < 7; ++field) {
            damage_lua.get_field(lua, 1, bubble_contact_fields[field]);
            bubble_contact_value(lua, fields + field * 2);
            damage_lua.set_top(lua, 1);
        }
        print_words(fields, 14);
        damage_lua.set_top(lua, 0);
    }
    for (size_t event = 0; event < state->count; ++event) {
        print_words(state->events[event], 12);
    }
}

static void probe_bubble_contact(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct_state)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*construct_object)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua9LuaObjectC1EPNS_8LuaStateE");
    void (*construct_table)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    void (*destroy_table)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaTableD1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*begin)(void *, void *) = (void (*)(void *, void *))find_function(
        "_ZN7GameLua18bubbleBeginContactEP9b2Contact");
    BubbleContactReference *state = &bubble_contact_reference;
    state->reference =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    uint32_t *wrapper = allocate(20);
    state->game = allocate(0x1860);
    if (!wrapper || !state->game) {
        fail("Cannot allocate native bubble contact context\n");
    }
    for (size_t word = 0; word < 0x1860 / 4; ++word) {
        state->game[word] = 0;
    }
    construct_state(wrapper);
    claim(wrapper);
    construct_object(state->game, wrapper);
    claim(state->game);
    void *lua = (void *)(uintptr_t)wrapper[2];
    for (size_t offset = 0x17f8; offset <= 0x1828; offset += 24) {
        construct_table((uint8_t *)state->game + offset, wrapper);
    }
    state->reference(state->game);
    damage_function(lua, 1, "trappedInBubble", bubble_contact_trapped);
    damage_function(lua, 1, "onCollision", bubble_contact_collision);
    damage_lua.set_top(lua, 0);
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{4, 4, UINT32_MAX, "bird"}, {6, 6, UINT32_MAX, "bubble"}};
    for (size_t object = 0; object < 2; ++object) {
        construct_table(state->objects[object] + 24, wrapper);
        state->objects[object][0] = object ? 11 : 3;
        state->objects[object][11] = (uint32_t)(uintptr_t)state->bodies[object];
        state->objects[object][32] = (uint32_t)(uintptr_t)names[object].bytes;
        state->bodies[object][6] = UINT32_C(0x3f800000);
        state->shapes[object][2] = UINT32_C(0x3f800000);
        state->fixtures[object][2] = (uint32_t)(uintptr_t)state->bodies[object];
        state->fixtures[object][3] = (uint32_t)(uintptr_t)state->shapes[object];
        state->fixtures[object][10] = (uint32_t)(uintptr_t)state->objects[object];
        state->reference((uint8_t *)state->game + 0x1810);
        state->reference(state->objects[object] + 24);
        damage_lua.set_field(lua, 1, names[object].bytes);
        damage_lua.set_top(lua, 0);
    }
    state->reference((uint8_t *)state->game + 0x17f8);
    state->reference((uint8_t *)state->game + 0x1810);
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_top(lua, 0);
    state->bodies[1][3] = UINT32_C(0x3fc00000);
    state->bodies[1][4] = UINT32_C(0xbe000000);
    state->contact[31] = 1;
    semihost(4, "AB_ANDROID_BUBBLE_CONTACT_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bubble_contact_case(sample, state->input);
        print_words(state->input, 11);
        state->count = 0;
        state->contact[1] = 7;
        state->contact[12] = (uint32_t)(uintptr_t)state->fixtures[state->input[3]];
        state->contact[13] = (uint32_t)(uintptr_t)state->fixtures[1u - state->input[3]];
        state->bodies[0][3] = state->bodies[0][4] = 0;
        state->bodies[0][16] = state->input[4];
        state->bodies[0][17] = state->input[5];
        state->objects[0][30] = state->input[7];
        state->objects[0][31] = state->input[8];
        ((uint8_t *)state->objects[0])[0xa0] = (uint8_t)state->input[6];
        state->reference((uint8_t *)state->game + 0x1828);
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 1, "bubble");
        damage_lua.set_top(lua, 0);
        for (size_t object = 0; object < 2; ++object) {
            state->reference(state->objects[object] + 24);
            for (size_t field = 0; field < 7; ++field) {
                uint32_t kind = object == 0 && field < 2
                                    ? state->input[1 + field]
                                    : (state->input[9] + (uint32_t)field) % 6u;
                bubble_contact_scalar(lua, kind);
                damage_lua.set_field(lua, 1, bubble_contact_fields[field]);
            }
            damage_lua.set_top(lua, 0);
        }
        begin(state->game, state->contact);
        bubble_contact_snapshot(lua);
    }
    for (size_t object = 0; object < 2; ++object) {
        destroy_table(state->objects[object] + 24);
    }
    for (size_t offset = 0x17f8; offset <= 0x1828; offset += 24) {
        destroy_table((uint8_t *)state->game + offset);
    }
    release(state->game);
    release(wrapper);
    semihost(4, "AB bubble contact reference complete\n");
}

#endif
