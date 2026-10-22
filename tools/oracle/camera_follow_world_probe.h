#ifndef AB_ORACLE_CAMERA_FOLLOW_WORLD_H
#define AB_ORACLE_CAMERA_FOLLOW_WORLD_H

#include "../../tests/world_raycast_cases.h"
#if defined(AB_ORACLE_CLASSIC_CAMERA)
#include "../../tests/classic_camera_cases.h"
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
#include "object_property_graph_probe.h"
#endif

static uint32_t *owner_world_wrapper;
static uint32_t owner_world_game[0x1a00 / 4];
static uint64_t owner_world_storage[16384];
static uint32_t owner_world_scene, owner_world_prediction_bird;

static uint32_t *owner_world_native_event(void *lua, uint32_t kind) {
    uint32_t size = 0, pc = predict_native_pc(lua, &size);
    int valid = kind == 9    ? size == 22 && pc == 19
                : kind == 10 ? size == 115 && (pc == 58 || pc == 73)
                             : size == 115 && pc == 109;
    int arguments = kind == 9 ? 4 : kind == 10 ? 2 : 1;
    if (!valid || damage_lua.get_top(lua) != arguments ||
        owner_native_count >= OWNER_FOLLOW_MAX_EVENTS)
        fail("Unexpected original world-follow prediction boundary\n");
    uint32_t *event = owner_native_events[owner_native_count++];
    for (uint32_t index = 0; index < OWNER_FOLLOW_EVENT_WORDS; ++index)
        event[index] = 0;
    event[0] = kind;
    event[1] = ++owner_native_counts[kind];
    event[2] = pc;
    event[3] = size;
    return event;
}

static int owner_world_native_ray(void *lua) {
    uint32_t *event = owner_world_native_event(lua, 9);
    if (lua != (void *)(uintptr_t)owner_world_wrapper[2])
        fail("World-follow query changed original Lua wrapper identity\n");
    for (int index = 0; index < 4; ++index)
        event[4 + (uint32_t)index] = owner_native_argument(lua, index + 1);
    int (*raycast)(void *, void *) =
        (int (*)(void *, void *))find_function("_ZN7GameLua7raycastEPN3lua8LuaStateE");
    int results = raycast(owner_world_game, owner_world_wrapper);
    if ((results != 1 && results != 6) || damage_lua.get_top(lua) != 4 + results ||
        damage_lua.type(lua, 5) != 1)
        fail("Invalid original world-follow ray result arity/type\n");
    event[8] = (uint32_t)results;
    event[9] = damage_lua.to_boolean(lua, 5) != 0;
    if (event[9]) {
        if (results != 6 || damage_lua.type(lua, 6) != 0)
            fail("Unnamed original world-follow fixture changed its name\n");
        for (int index = 0; index < 4; ++index)
            event[10 + (uint32_t)index] = owner_native_argument(lua, 7 + index);
    } else if (results != 1) {
        fail("Original world-follow miss supplied extra fields\n");
    }
    return results;
}

static int owner_world_native_radius(void *lua) {
    uint32_t *event = owner_world_native_event(lua, 10);
    if (!damage_named(lua, 2, "radius"))
        fail("Unexpected original world-follow property key\n");
    FloatBits id = {.bits = owner_native_number(lua, 1, "_id")};
    if (id.number != (float)owner_world_prediction_bird ||
        (owner_world_prediction_bird != 1 && owner_world_prediction_bird != 2))
        fail("Original world-follow radius changed bird identity\n");
    owner_native_identity(lua, 1,
                          owner_world_prediction_bird == 1 ? "_ab_owner_world_script1"
                                                           : "_ab_owner_world_script2");
    event[4] = owner_world_prediction_bird;
    float radius;
#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    damage_lua.get_field(lua, -10002, "_ab_level_get");
    push_value(lua, 1);
    push_value(lua, 2);
    owner_native_checked(lua, 2, 1);
    if (damage_lua.type(lua, 3) != 3)
        fail("Original world-follow property graph returned nonnumeric radius\n");
    radius = damage_lua.to_number(lua, 3);
#else
    radius = 0.25f + (float)(owner_world_scene % 4u) * 0.125f;
    damage_lua.push_number(lua, radius);
#endif
    event[5] = owner_follow_word(radius);
    return 1;
}

static int owner_world_native_print(void *lua) {
    if (damage_lua.type(lua, 1) != 3)
        return owner_end_native_print(lua);
    uint32_t *event = owner_world_native_event(lua, 11);
    event[4] = owner_native_argument(lua, 1);
    return 0;
}

static void owner_world_native_script(void *lua, int bird, uint32_t id) {
    int top = damage_lua.get_top(lua);
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, (float)id);
    damage_lua.set_field(lua, top + 1, "_id");
    damage_function(lua, top + 1, "get", owner_world_native_radius);
    push_value(lua, top + 1);
    damage_lua.set_field(
        lua, -10002, id == 1 ? "_ab_owner_world_script1" : "_ab_owner_world_script2");
#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    damage_lua.push_string(lua, id == 1 ? "bird1" : "bird2");
    damage_lua.set_field(lua, bird, "name");
    damage_lua.push_string(lua, "Bird");
    damage_lua.set_field(lua, bird, "definition");
    damage_lua.push_number(lua, -1.0f);
    damage_lua.set_field(lua, bird, "width");
    damage_lua.push_number(lua, -1.0f);
    damage_lua.set_field(lua, bird, "height");
    damage_lua.push_number(lua, 0.25f + (float)(owner_world_scene % 4u) * 0.125f);
    damage_lua.set_field(lua, bird, "radius");
    damage_lua.get_field(lua, -10002, "_ab_level_init");
    push_value(lua, top + 1);
    push_value(lua, bird);
    owner_native_checked(lua, 2, 0);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, -1, "world");
    push_value(lua, bird);
    damage_lua.set_field(lua, top + 3, id == 1 ? "bird1" : "bird2");
    damage_lua.set_top(lua, top + 1);
#endif
    damage_lua.set_field(lua, bird, "gameObjectScript");
}

#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
static void owner_world_native_properties(void *lua) {
    load_object_property_graph(lua);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "Bird");
    damage_lua.set_field(lua, -10002, "_ab_owner_property_blocks");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "_ab_owner_property_blocks");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "materials");
    damage_lua.set_field(lua, -10002, "blockTable");
}
#endif

static int owner_world_native_predict_result(void *lua, uint32_t *event) {
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    owner_world_prediction_bird = event[4];
    damage_lua.get_field(lua, -10002, "_ab_owner_world_predict");
    push_value(lua, 1);
    push_value(lua, 2);
    owner_native_checked(lua, 2, 3);
    if (damage_lua.get_top(lua) != 5 || damage_lua.type(lua, 4) != 0)
        fail("Unexpected original world-follow prediction result/name\n");
    if (damage_lua.type(lua, 3) == 5) {
        event[13] = 1;
        event[10] = owner_native_number(lua, 3, "x");
        event[11] = owner_native_number(lua, 3, "y");
    } else if (damage_lua.type(lua, 3) != 0) {
        fail("Unexpected original world-follow prediction point type\n");
    }
    event[12] = owner_native_argument(lua, 5);
    return 3;
}

static void *owner_world_native_state(void) {
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct)(void *) = (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    owner_world_wrapper = allocate(20);
    if (!owner_world_wrapper)
        fail("Cannot allocate original world-follow Lua wrapper\n");
    construct(owner_world_wrapper);
    claim(owner_world_wrapper);
    return (void *)(uintptr_t)owner_world_wrapper[2];
}

#if defined(AB_ORACLE_CLASSIC_CAMERA)
#if defined(AB_ORACLE_CLASSIC_CAMERA_LIVE)
static void owner_classic_native_publish(void *lua) {
    for (uint32_t role = 0; role < 2; ++role) {
        uint32_t values[5];
        classic_camera_frame_values(owner_native_config, owner_native_command[11], role,
                                    values);
        damage_lua.get_field(lua, -10002, role ? "_ab_owner_bird2" : "_ab_owner_bird1");
        damage_number(lua, 1, "x", values[0]);
        damage_number(lua, 1, "y", values[1]);
        damage_lua.get_field(lua, 1, "launchVelocity");
        int kind = damage_lua.type(lua, 2);
        if (values[4]) {
            if (kind == 0) {
                damage_lua.set_top(lua, 1);
                owner_native_vector(lua, owner_follow_number(values[2]),
                                    owner_follow_number(values[3]));
                damage_lua.set_field(lua, 1, "launchVelocity");
            } else if (kind == 5) {
                damage_number(lua, 2, "x", values[2]);
                damage_number(lua, 2, "y", values[3]);
            } else {
                fail("Invalid original launched-vector publication\n");
            }
        } else if (kind != 0) {
            fail("Original launch appeared before its publication boundary\n");
        }
        damage_lua.set_top(lua, 0);
    }
}
#endif

static void owner_classic_native_order(uint32_t order[3]) {
    void *lua = (void *)(uintptr_t)owner_world_wrapper[2];
    if (damage_lua.get_top(lua))
        fail("Unexpected stack before original level constructor traversal\n");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, 1.0f);
    damage_lua.set_field(lua, 1, "bird1");
    damage_lua.push_number(lua, 2.0f);
    damage_lua.set_field(lua, 1, "bird2");
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    uint32_t count = 0;
    damage_lua.push_nil(lua);
    while (next(lua, 1)) {
        if (count >= 2 || damage_lua.type(lua, 3) != 3)
            fail("Invalid original level constructor entry\n");
        uint32_t value = (uint32_t)damage_lua.to_number(lua, 3);
        if (value != 1 && value != 2)
            fail("Unknown original level constructor role\n");
        order[count++] = value - 1u;
        damage_lua.set_top(lua, 2);
    }
    if (count != 2 || order[0] == order[1])
        fail("Incomplete original level constructor traversal\n");
    order[2] = 2;
    damage_lua.set_top(lua, 0);
}

static void owner_classic_native_compact(uint32_t output[80]) {
    uint32_t count = 0;
    for (uint32_t index = 0; index < owner_native_count; ++index) {
        uint32_t kind = owner_native_events[index][0];
        if (kind != 4 && kind != 7 && kind != 9 && kind != 11)
            continue;
        if (count != index) {
            for (uint32_t word = 0; word < OWNER_FOLLOW_EVENT_WORDS; ++word)
                owner_native_events[count][word] = owner_native_events[index][word];
        }
        ++count;
    }
    owner_native_count = count;
    output[46] = output[48] = output[49] = 0;
    output[50] = count;
}
#endif

static void owner_world_native_open(uint32_t scene) {
    void (*construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void *(*create_fixture)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN6b2Body13CreateFixtureEPK12b2FixtureDef");
    void (*set_box)(void *, float, float) = (void (*)(
        void *, float, float))find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    const uint32_t *circle = (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1);
    const uint32_t *polygon = (const uint32_t *)find_symbol("_ZTV14b2PolygonShape", 1);
#if defined(AB_ORACLE_CLASSIC_CAMERA)
    uint32_t order[CLASSIC_CAMERA_BODIES];
    owner_classic_native_order(order);
    const uint32_t gravity[] = {0, UINT32_C(0x41a00000)};
    const uint32_t bodies = CLASSIC_CAMERA_BODIES;
#else
    const uint32_t gravity[] = {0, 0};
    const uint32_t bodies = WORLD_RAY_BODIES;
#endif
    uint32_t *world = (uint32_t *)owner_world_storage;
    construct(world, gravity);
    owner_world_game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
    owner_world_scene = scene;
    for (uint32_t index = 0; index < bodies; ++index) {
        uint32_t row[WORLD_RAY_BODY_WORDS];
#if defined(AB_ORACLE_CLASSIC_CAMERA)
        classic_camera_body(order[index], row);
#else
        world_raycast_body(scene, index, row);
#endif
        print_words(row, WORLD_RAY_BODY_WORDS);
        uint32_t body_definition[13] = {0};
        body_definition[0] = row[1];
        body_definition[1] = row[2];
        body_definition[2] = row[3];
        body_definition[3] = row[4];
        body_definition[9] = UINT32_C(0x00000101);
        body_definition[10] = row[5];
        body_definition[11] = row[0];
        body_definition[12] = UINT32_C(0x3f800000);
#if defined(AB_ORACLE_CLASSIC_CAMERA)
        body_definition[8] = UINT32_C(0x3f800000);
        body_definition[11] = 0;
#endif
        void *body = create_body(world, body_definition);
        if (!body)
            fail("Cannot construct original world-follow body\n");
        uint32_t shape[38] = {0};
        shape[0] = (uint32_t)(uintptr_t)((row[6] ? polygon : circle) + 2);
        shape[1] = row[6];
        shape[2] = row[7];
        if (row[6]) {
            FloatBits x = {.bits = row[8]}, y = {.bits = row[9]};
            set_box(shape, x.number, y.number);
        } else {
            shape[3] = row[8];
            shape[4] = row[9];
        }
        uint32_t fixture[] = {
            (uint32_t)(uintptr_t)shape, 0,      UINT32_C(0x3e4ccccd), 0, 0,
            row[10] | row[11] << 16,    row[12]};
#if defined(AB_ORACLE_CLASSIC_CAMERA)
        fixture[2] = world_ray_word(order[index] == 2 ? 0.8f : 0.3f);
        fixture[4] = world_ray_word(order[index] == 2 ? 0.0f : 1.0f);
#endif
        if (!create_fixture(body, fixture))
            fail("Cannot construct original world-follow fixture\n");
#if defined(AB_ORACLE_CLASSIC_CAMERA)
        if (order[index] != 2) {
            void (*transform)(void *, const void *, float) =
                (void (*)(void *, const void *, float))find_function(
                    "_ZN6b2Body12SetTransformERK6b2Vec2f");
            const uint32_t position[] = {row[2], row[3]};
            transform(body, position, 0.0f);
        }
#endif
    }
    if (scene & 16u)
        world[0x191d4 / 4] |= 2u;
}

static void owner_world_native_close(void) {
    uint32_t *world = (uint32_t *)owner_world_storage;
    world[0x191d4 / 4] &= ~2u;
    void (*destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    destroy(world);
}

static void owner_world_native_release(void) {
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    release(owner_world_wrapper);
}

#endif
