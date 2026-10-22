#ifndef AB_ORACLE_GAME_CONTACT_WORLD_H
#define AB_ORACLE_GAME_CONTACT_WORLD_H

#include "../../tests/game_contact_world_cases.h"

typedef struct {
    uint32_t *game;
    uint32_t objects[2][50], nodes[2][6], pairs[64][2];
    uint32_t bodies[2], input[8], sample, frame;
    uint32_t events[64][7], event_count, begins, ends, posts, destroyed;
    void (*begin)(void *, void *);
    void (*end)(void *, void *);
    void (*pre_solve)(void *, void *, const void *);
} ContactWorldReference;

static ContactWorldReference contact_world_reference;

static void contact_world_event(void *lua, uint32_t kind) {
    ContactWorldReference *state = &contact_world_reference;
    int arguments = kind == 1 ? 7 : 2;
    if (damage_lua.get_top(lua) != arguments || state->event_count >= 64) {
        fail("Unexpected native world game-contact callback\n");
    }
    uint32_t *event = state->events[state->event_count++];
    for (size_t field = 0; field < 7; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    for (int argument = 1; argument <= 2; ++argument) {
        if (!damage_named(lua, argument, "first") &&
            !damage_named(lua, argument, "second")) {
            fail("Unknown native world contact object name\n");
        }
        event[argument] = damage_object_id(lua, argument);
    }
    if (kind == 1) {
        for (int argument = 3; argument <= 6; ++argument) {
            if (damage_lua.type(lua, argument) != 3) {
                fail("Invalid native world contact geometry type\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, argument)};
            event[argument] = value.bits;
        }
    }
}

static int contact_world_collision(void *lua) {
    ContactWorldReference *state = &contact_world_reference;
    contact_world_event(lua, 1);
    ++state->begins;
    damage_lua.get_field(lua, 7, "shouldDisableContact");
    if (damage_lua.type(lua, -1) != 1 || damage_lua.to_boolean(lua, -1)) {
        fail("Native world contact response was not initialized false\n");
    }
    damage_lua.set_top(lua, 7);
    damage_lua.push_boolean(lua, (state->input[0] & CONTACT_WORLD_DISABLE) != 0);
    damage_lua.set_field(lua, 7, "shouldDisableContact");
    if (state->input[0] & CONTACT_WORLD_OFFER) {
        uint32_t velocity[2];
        game_contact_world_proposal(state->sample, state->frame, velocity);
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_string(lua, "second");
        damage_lua.set_field(lua, -2, "birdName");
        damage_number(lua, -2, "afterCollisionVelocityX", velocity[0]);
        damage_number(lua, -2, "afterCollisionVelocityY", velocity[1]);
        damage_lua.set_field(lua, 7, "newCollisionPathParameters");
        void (*set_index)(void *, int, int) =
            (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
        damage_lua.create_table(lua, 2, 0);
        damage_lua.push_string(lua, "first");
        set_index(lua, -2, 1);
        damage_lua.push_string(lua, "second");
        set_index(lua, -2, 2);
        damage_lua.set_field(lua, 7, "glueContactPair");
    }
    return 0;
}

static int contact_world_ended(void *lua) {
    contact_world_event(lua, 2);
    ++contact_world_reference.ends;
    return 0;
}

static void contact_world_begin(void *listener, void *contact) {
    (void)listener;
    contact_world_reference.begin(contact_world_reference.game, contact);
}

static void contact_world_end(void *listener, void *contact) {
    (void)listener;
    contact_world_reference.end(contact_world_reference.game, contact);
}

static void contact_world_pre(void *listener, void *contact, const void *previous) {
    (void)listener;
    contact_world_reference.pre_solve(contact_world_reference.game, contact, previous);
}

static void contact_world_post(void *listener, const void *contact,
                               const void *impulse) {
    (void)listener;
    (void)contact;
    (void)impulse;
    ++contact_world_reference.posts;
}

static void contact_world_goodbye(void *listener, const void *fixture) {
    (void)listener;
    (void)fixture;
    ++contact_world_reference.destroyed;
}

static void contact_world_snapshot(const uint32_t *world) {
    ContactWorldReference *state = &contact_world_reference;
    uint32_t queue_begin = (uint32_t)(uintptr_t)state->pairs;
    uint32_t queue_end = state->game[0x3c / 4];
    if (queue_end < queue_begin || (queue_end - queue_begin) % 8 ||
        queue_end - queue_begin > sizeof(state->pairs) || world[0x19218 / 4] > 1) {
        fail("Native game-contact world snapshot exceeds bounds\n");
    }
    uint32_t queued = (queue_end - queue_begin) / 8;
    const uint32_t summary[] = {state->sample,
                                state->frame,
                                state->event_count,
                                state->begins,
                                state->ends,
                                state->posts,
                                queued,
                                state->objects[0][40] & 255u,
                                state->objects[0][30],
                                state->objects[0][31],
                                state->objects[1][40] & 255u,
                                state->objects[1][30],
                                state->objects[1][31],
                                world[0x19218 / 4]};
    print_words(summary, 14);
    for (size_t index = 0; index < 2; ++index) {
        const uint32_t *body = (const uint32_t *)(uintptr_t)state->bodies[index];
        const uint32_t values[] = {body[0],  (body[1] >> 1) & 1u,
                                   body[3],  body[4],
                                   body[5],  body[6],
                                   body[16], body[17],
                                   body[18], body[29],
                                   body[30], body[31],
                                   body[32], body[36],
                                   body[13], body[14],
                                   body[9],  body[10],
                                   body[11], body[12]};
        print_words(values, 20);
    }
    uint32_t contact_values[12] = {0};
    if (world[0x19218 / 4]) {
        const uint32_t *contact = (const uint32_t *)(uintptr_t)world[0x19214 / 4];
        contact_values[0] = contact[1];
        contact_values[1] = contact[31];
        if (contact[31] > 1) {
            fail("Expected a single native circle contact point\n");
        }
        if (contact[31]) {
            contact_values[2] = contact[30];
            for (size_t field = 0; field < 4; ++field) {
                contact_values[3 + field] = contact[26 + field];
            }
            for (size_t field = 0; field < 5; ++field) {
                contact_values[7 + field] = contact[16 + field];
            }
        }
    }
    print_words(contact_values, 12);
    for (size_t event = 0; event < state->event_count; ++event) {
        print_words(state->events[event], 7);
    }
    for (size_t index = 0; index < queued; ++index) {
        uint32_t pair[2];
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            uint32_t body = state->pairs[index][endpoint];
            if (body != state->bodies[0] && body != state->bodies[1]) {
                fail("Unexpected native queued glue endpoint\n");
            }
            pair[endpoint] = body == state->bodies[0] ? 0 : 1;
        }
        print_words(pair, 2);
    }
}

static void probe_game_contact_world(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*state_construct)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*object_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua9LuaObjectC1EPNS_8LuaStateE");
    void (*table_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    void (*table_destroy)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaTableD1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    void (*setup)(void *) =
        (void (*)(void *))find_function("_ZN7GameLua19setupCollisionTableEv");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*world_destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void *(*create_fixture)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN6b2Body13CreateFixtureEPK12b2FixtureDef");
    void (*step)(void *, float, int, int) =
        (void (*)(void *, float, int, int))find_function("_ZN7b2World4StepEfii");
    void (*set_transform)(void *, const void *, float) =
        (void (*)(void *, const void *, float))find_function(
            "_ZN6b2Body12SetTransformERK6b2Vec2f");
    void (*destroy_body)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN7b2World11DestroyBodyEP6b2Body");
    ContactWorldReference *state = &contact_world_reference;
    state->begin = (void (*)(void *, void *))find_function(
        "_ZN7GameLua12BeginContactEP9b2Contact");
    state->end =
        (void (*)(void *, void *))find_function("_ZN7GameLua10EndContactEP9b2Contact");
    state->pre_solve = (void (*)(void *, void *, const void *))find_function(
        "_ZN7GameLua8PreSolveEP9b2ContactPK10b2Manifold");
    uint32_t *wrapper = allocate(20);
    state->game = allocate(0x1860);
    if (!wrapper || !state->game) {
        fail("Cannot allocate native game-contact world context\n");
    }
    for (size_t word = 0; word < 0x1860 / 4; ++word) {
        state->game[word] = 0;
    }
    state_construct(wrapper);
    claim(wrapper);
    object_construct(state->game, wrapper);
    claim(state->game);
    table_construct((uint8_t *)state->game + 0x1810, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    damage_lua.set_top(lua, 0);
    reference(state->game);
    damage_function(lua, -2, "onCollision", contact_world_collision);
    damage_function(lua, -2, "onEndContact", contact_world_ended);
    damage_lua.set_top(lua, 0);
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{5, 5, UINT32_MAX, "first"}, {6, 6, UINT32_MAX, "second"}};
    uint32_t header = (uint32_t)(uintptr_t)((uint8_t *)state->game + 0x17a8);
    state->nodes[0][0] = 1;
    state->nodes[0][1] = header;
    state->nodes[0][3] = (uint32_t)(uintptr_t)state->nodes[1];
    state->nodes[1][1] = (uint32_t)(uintptr_t)state->nodes[0];
    for (size_t index = 0; index < 2; ++index) {
        state->nodes[index][4] = (uint32_t)(uintptr_t)names[index].bytes;
        state->nodes[index][5] = (uint32_t)(uintptr_t)state->objects[index];
    }
    state->game[0x17ac / 4] = (uint32_t)(uintptr_t)state->nodes[0];
    state->game[0x17b0 / 4] = (uint32_t)(uintptr_t)state->nodes[0];
    state->game[0x17b4 / 4] = (uint32_t)(uintptr_t)state->nodes[1];
    state->game[0x17b8 / 4] = 2;
    uint32_t listener_vtable[] = {0,
                                  0,
                                  (uint32_t)(uintptr_t)contact_world_begin,
                                  (uint32_t)(uintptr_t)contact_world_end,
                                  (uint32_t)(uintptr_t)contact_world_pre,
                                  (uint32_t)(uintptr_t)contact_world_post};
    uint32_t listener[] = {(uint32_t)(uintptr_t)listener_vtable};
    uint32_t destruction_vtable[] = {0, 0, 0,
                                     (uint32_t)(uintptr_t)contact_world_goodbye};
    uint32_t destruction[] = {(uint32_t)(uintptr_t)destruction_vtable};
    const uint32_t *circle = (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1);
    static uint64_t world_storage[16384];
    uint32_t *world = (uint32_t *)world_storage;
    const uint32_t gravity[2] = {0, 0};
    semihost(4, "AB_ANDROID_GAME_CONTACT_WORLD_V1 64 16\n");
    for (uint32_t sample = 0; sample < 64; ++sample) {
        state->sample = sample;
        game_contact_world_case(sample, state->input);
        print_words(state->input, 8);
        uint32_t flags = state->input[0];
        state->event_count = state->begins = state->ends = state->posts =
            state->destroyed = 0;
        setup(state->game);
        ((uint8_t *)state->game)[0x185d] = (flags & CONTACT_WORLD_BEGIN_DISABLED) != 0;
        state->game[0x38 / 4] = state->game[0x3c / 4] =
            (uint32_t)(uintptr_t)state->pairs;
        state->game[0x40 / 4] = (uint32_t)(uintptr_t)(state->pairs + 64);
        reference((uint8_t *)state->game + 0x1810);
        for (size_t index = 0; index < 2; ++index) {
            if (flags & ((uint32_t)CONTACT_WORLD_FIRST_SCRIPT << index)) {
                damage_lua.create_table(lua, 0, 0);
            } else {
                damage_lua.push_nil(lua);
            }
            damage_lua.set_field(lua, 1, index ? "second" : "first");
        }
        damage_lua.set_top(lua, 0);
        world_construct(world, gravity);
        world[0x19220 / 4] = (uint32_t)(uintptr_t)listener;
        world[0x19244 / 4] = (uint32_t)(uintptr_t)destruction;
        ((uint8_t *)world)[0x19240] = 1;
        ((uint8_t *)world)[0x19250] = 1;
        ((uint8_t *)world)[0x19251] = 0;
        for (size_t index = 0; index < 2; ++index) {
            for (size_t word = 0; word < 50; ++word) {
                state->objects[index][word] = 0;
            }
            uint32_t definition[13] = {0};
            definition[0] = index ? 2 : 0;
            definition[1] = index ? state->input[3] : 0;
            definition[2] = index ? state->input[4] : 0;
            definition[4] = index ? state->input[5] : 0;
            definition[5] = index ? state->input[6] : 0;
            definition[9] = 0x100;
            definition[10] = 1;
            definition[12] = UINT32_C(0x3f800000);
            void *body = create_body(world, definition);
            if (!body) {
                fail("Native game-contact body creation failed\n");
            }
            state->bodies[index] = (uint32_t)(uintptr_t)body;
            state->objects[index][11] = state->bodies[index];
            state->objects[index][32] = (uint32_t)(uintptr_t)names[index].bytes;
            uint32_t shape[] = {(uint32_t)(uintptr_t)(circle + 2), 0,
                                state->input[1 + index], 0, 0};
            uint32_t fixture[] = {
                (uint32_t)(uintptr_t)shape,
                flags & ((uint32_t)CONTACT_WORLD_FIRST_OBJECT << index)
                    ? (uint32_t)(uintptr_t)state->objects[index]
                    : 0,
                0,
                0,
                index ? UINT32_C(0x3f800000) : 0,
                UINT32_C(0x10000),
                flags & CONTACT_WORLD_MASKED ? 0 : 0xffff};
            if (!create_fixture(body, fixture)) {
                fail("Native game-contact fixture creation failed\n");
            }
        }
        for (uint32_t frame = 0; frame < 16; ++frame) {
            state->frame = frame;
            state->event_count = 0;
            step(world, 1.0f / 30, 8, 3);
            contact_world_snapshot(world);
        }
        state->frame = 16;
        state->event_count = 0;
        ((uint8_t *)state->game)[0x185d] = 1;
        const uint32_t position[] = {UINT32_C(0x41200000), 0};
        set_transform((void *)(uintptr_t)state->bodies[1], position, 0);
        step(world, 0, 8, 3);
        contact_world_snapshot(world);
        destroy_body(world, (void *)(uintptr_t)state->bodies[1]);
        const uint32_t final[] = {state->ends, state->destroyed, world[0x19230 / 4]};
        print_words(final, 3);
        world[0x19220 / 4] = world[0x19244 / 4] = 0;
        world_destroy(world);
    }
    table_destroy((uint8_t *)state->game + 0x1810);
    release(state->game);
    release(wrapper);
    semihost(4, "AB game contact world reference complete\n");
}

#endif
