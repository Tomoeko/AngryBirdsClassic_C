#ifndef AB_ORACLE_BIRD_ROUTE_H
#define AB_ORACLE_BIRD_ROUTE_H

static uint32_t bird_route_input[20];
static uint32_t bird_route_argument_types;

static int bird_route_object(void *lua) {
    damage_lua.get_field(lua, -10002,
                         damage_object_id(lua, 1) == 1 ? "first" : "second");
    return 1;
}

static int bird_route_definition(void *lua) {
    unsigned id = damage_object_id(lua, 1);
    (void)bird_damage_definition(lua);
    damage_lua.push_number(lua, (float)bird_route_input[id == 1 ? 18 : 19]);
    damage_lua.set_field(lua, -2, "collider");
    return 1;
}

static int bird_route_callback(void *lua) {
    for (int argument = 3; argument <= 8; ++argument) {
        bird_route_argument_types |= (uint32_t)damage_lua.type(lua, argument)
                                     << ((unsigned)(argument - 3) * 4);
    }
    return bird_damage_callback(lua);
}

static void prepare_bird_route_game(void *lua) {
    prepare_bird_damage_game(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    int game = damage_lua.get_top(lua);
    damage_function(lua, game, "getObject", bird_route_object);
    damage_function(lua, game, "getObjectDefinition", bird_route_definition);
    damage_lua.get_field(lua, -1, "blockTable");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, -2, "eagle");
    damage_lua.push_number(lua, 3);
    damage_lua.set_field(lua, -2, "bird");
    damage_lua.push_number(lua, 10);
    damage_lua.set_field(lua, -2, "glue");
    damage_lua.set_field(lua, -2, "collider_types");
    damage_lua.set_top(lua, 0);
}

static void print_bird_route_results(void *lua) {
    uint32_t output[10] = {0};
    damage_lua.get_field(lua, -10002, "second");
    damage_lua.get_field(lua, -1, "strength");
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    FloatBits strength = {.number = damage_lua.to_number(lua, -1)};
    output[1] = strength.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -1, "deadBlocks");
    damage_lua.get_field(lua, -1, "second");
    output[2] = damage_lua.type(lua, -1) == 5;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_route_result");
    damage_lua.get_field(lua, 1, "shouldDisableContact");
    output[3] = (uint32_t)damage_lua.type(lua, -1);
    output[4] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "newCollisionPathParameters");
    output[5] = (uint32_t)damage_lua.type(lua, -1);
    if (output[5] == 5) {
        damage_lua.get_field(lua, 2, "birdName");
        output[6] = damage_object_id(lua, -1);
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "afterCollisionVelocityX");
        FloatBits horizontal = {.number = damage_lua.to_number(lua, -1)};
        output[7] = horizontal.bits;
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "afterCollisionVelocityY");
        FloatBits vertical = {.number = damage_lua.to_number(lua, -1)};
        output[8] = vertical.bits;
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "glueContactPair");
    output[9] = (uint32_t)damage_lua.type(lua, -1);
    damage_lua.set_top(lua, 0);
    print_words(output, 10);
    print_words(&bird_route_argument_types, 1);
    uint32_t count = (uint32_t)damage_event_count;
    print_words(&count, 1);
    for (size_t event = 0; event < damage_event_count; ++event) {
        print_words(damage_events[event], 9);
    }
}

static void probe_bird_route(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate enclosing bird collision reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    damage_function(lua, -10002, "requireFile", damage_require);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/vector2.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/museum-collisions.lua");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "birdCollision", bird_route_callback);
    damage_lua.set_field(lua, -10002, "CollisionCallbacks");
    uint32_t state = UINT32_C(0x520b172e);
    semihost(4, "AB_ANDROID_BIRD_ROUTE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        bird_route_input[0] = random_float(&state, 122, 12, 1);
        bird_route_input[1] = random_float(&state, 122, 12, 1);
        bird_route_input[2] = index % 17 ? random_float(&state, 123, 9, 0) : 0;
        bird_route_input[3] = random_float(&state, 114, 17, 0);
        bird_route_input[4] = random_float(&state, 115, 14, 0);
        bird_route_input[5] = index % 19 ? random_float(&state, 124, 7, 0) : 0;
        bird_route_input[6] = random_float(&state, 123, 7, 0);
        bird_route_input[7] = random_float(&state, 123, 6, 0);
        uint32_t flags = (uint32_t)(index & 511);
        if ((flags & 2) && !(flags & 4) && !(flags & 8)) {
            flags &= ~UINT32_C(2);
        }
        bird_route_input[8] = flags;
        for (size_t field = 9; field < 18; ++field) {
            bird_route_input[field] = random_float(&state, 120, 10, field != 17);
        }
        if (index % 13 == 0) {
            bird_route_input[15] = bird_route_input[0];
            bird_route_input[16] = bird_route_input[1];
        }
        bool pair = (index & 2) != 0;
        bird_route_input[18] = index & 4 ? 1 : 3;
        bird_route_input[19] = pair ? (index & 8 ? 1u : 3u) : (index & 8 ? 4u : 0u);
        for (size_t field = 0; field < 15; ++field) {
            bird_damage_input[field] = bird_route_input[field];
        }
        prepare_bird_damage_objects(lua);
        damage_lua.get_field(lua, -10002, "second");
        damage_number(lua, -2, "xVel", bird_route_input[15]);
        damage_number(lua, -2, "yVel", bird_route_input[16]);
        damage_number(lua, -2, "mass", bird_route_input[17]);
        damage_lua.set_top(lua, 0);
        prepare_bird_route_game(lua);
        bird_route_argument_types = 0;
        damage_event_count = 0;
        for (size_t event = 0; event < 3; ++event) {
            for (size_t field = 0; field < 9; ++field) {
                damage_events[event][field] = 0;
            }
        }
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -2, "shouldDisableContact");
        damage_lua.set_field(lua, -10002, "_ab_route_result");
        print_words(bird_route_input, 20);
        damage_lua.get_field(lua, -10002, "MuseumCollisions");
        damage_lua.get_field(lua, -1, "onCollision");
        damage_lua.push_string(lua, index & 1 ? "second" : "first");
        damage_lua.push_string(lua, index & 1 ? "first" : "second");
        for (size_t field = 9; field < 13; ++field) {
            FloatBits value = {.bits = bird_route_input[field]};
            damage_lua.push_number(lua, value.number);
        }
        damage_lua.get_field(lua, -10002, "_ab_route_result");
        if (damage_lua.call(lua, 7, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original enclosing bird collision failed\n");
        }
        damage_lua.set_top(lua, 0);
        print_bird_route_results(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird route reference complete\n");
}

#endif
