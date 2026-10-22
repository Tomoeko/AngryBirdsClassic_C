#ifndef AB_ORACLE_MUSEUM_GLUE_ROUTE_H
#define AB_ORACLE_MUSEUM_GLUE_ROUTE_H

#include "../../tests/museum_glue_route_cases.h"

static uint32_t glue_route_input[9], glue_route_events[12][4], glue_route_count;

static uint32_t glue_route_name(void *lua, int argument) {
    if (damage_lua.type(lua, argument) == 5) {
        damage_lua.get_field(lua, argument, "name");
        uint32_t name = museum_glue_name_id(lua, -1);
        damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
        return name;
    }
    return museum_glue_name_id(lua, argument);
}

static void glue_route_event(void *lua, uint32_t kind) {
    if (glue_route_count >= 12) {
        fail("Enclosing glue callback event overflow\n");
    }
    uint32_t *event = glue_route_events[glue_route_count++];
    event[0] = kind;
    event[1] = glue_route_name(lua, 1);
    event[2] = event[3] = 0;
    if (kind == 3 || kind == 4) {
        event[2] = glue_route_name(lua, 2);
        event[3] = (uint32_t)damage_lua.get_top(lua);
    }
}

static int glue_route_object(void *lua) {
    glue_route_event(lua, 1);
    uint32_t name = glue_route_name(lua, 1);
    if (!name || name > 3) {
        fail("Unexpected enclosing glue object lookup\n");
    }
    damage_lua.get_field(lua, -10002, museum_glue_names[name - 1]);
    return 1;
}

static int glue_route_definition(void *lua) {
    glue_route_event(lua, 2);
    uint32_t name = glue_route_name(lua, 1);
    if (name != 1 && name != 2) {
        fail("Unexpected enclosing glue definition lookup\n");
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, (float)glue_route_input[name]);
    damage_lua.set_field(lua, -2, "collider");
    return 1;
}

static void glue_route_primary_dead(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -1, "deadBlocks");
    for (uint32_t object = 0; object < 2; ++object) {
        if (glue_route_input[7] & (1u << object)) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, -2, museum_glue_names[object]);
        }
    }
    damage_lua.set_top(lua, top);
}

static int glue_route_bird(void *lua) {
    glue_route_event(lua, 3);
    glue_route_primary_dead(lua);
    damage_lua.push_boolean(lua, (int)glue_route_input[6]);
    damage_lua.push_nil(lua);
    return 2;
}

static int glue_route_block(void *lua) {
    glue_route_event(lua, 4);
    glue_route_primary_dead(lua);
    return 0;
}

static int glue_route_velocity(void *lua) {
    glue_route_event(lua, 5);
    for (int argument = 2; argument <= 3; ++argument) {
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        glue_route_events[glue_route_count - 1][argument] = value.bits;
    }
    return 0;
}

static void probe_museum_glue_route(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate enclosing glue reference state\n");
    }
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "lua");
    damage_function(lua, -10002, "requireFile", damage_require);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/museum-collisions.lua");
    damage_function(lua, -10002, "_ab_glue_route_bird", glue_route_bird);
    damage_function(lua, -10002, "_ab_glue_route_block", glue_route_block);
    damage_lua.get_field(lua, -10002, "MuseumCollisions");
    damage_lua.get_field(lua, 1, "onCollision");
    /* Stripped names make lua_setupvalue reject valid slots. Bind only the
     * fresh closed primary-helper UpVals through checked pinned ARM32 fields.
     * The authored C closures are rooted globally; the original glue closure,
     * prototype and all original instructions remain unchanged. */
    const uint8_t *closure = damage_lua.to_pointer(lua, 2);
    if (!closure || closure[6] || closure[7] != 4) {
        fail("Unexpected original enclosing collision closure\n");
    }
    static const char *const helpers[] = {"_ab_glue_route_bird",
                                          "_ab_glue_route_block"};
    for (size_t slot = 0; slot < 2; ++slot) {
        damage_lua.get_field(lua, -10002, helpers[slot]);
        const uint8_t *replacement = damage_lua.to_pointer(lua, -1);
        const uint32_t *upvalue =
            (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[6 + slot];
        if (!replacement || !replacement[6] || replacement[7] || !upvalue ||
            upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3) ||
            (((const uint8_t *)upvalue)[5] & 4u)) {
            fail("Expected a fresh closed primary-helper upvalue and rooted C "
                 "closure\n");
        }
        uint32_t *value = (uint32_t *)(uintptr_t)upvalue[2];
        const uint8_t *original = (const uint8_t *)(uintptr_t)value[0];
        if (value[1] != 6 || !original || original[6] || original[7] != 1) {
            fail("Expected original primary Lua helper closure\n");
        }
        value[0] = (uint32_t)(uintptr_t)replacement;
        value[1] = 6;
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_MUSEUM_GLUE_ROUTE_V1 8192\n");
    for (uint32_t sample = 0; sample < 8192; ++sample) {
        museum_glue_route_case(sample, glue_route_input);
        print_words(glue_route_input, 9);
        glue_route_count = 0;
        for (uint32_t object = 0; object < 3; ++object) {
            damage_lua.create_table(lua, 0, 5);
            damage_lua.push_string(lua, museum_glue_names[object]);
            damage_lua.set_field(lua, -2, "name");
            damage_lua.push_string(lua, "plunger");
            damage_lua.set_field(lua, -2, "plungerName");
            museum_glue_value(lua, object < 2 ? glue_route_input[8] : 0);
            damage_lua.set_field(lua, -2, "grabber");
            damage_lua.set_field(lua, -10002, museum_glue_names[object]);
        }
        damage_lua.create_table(lua, 0, 4);
        damage_function(lua, -2, "getObject", glue_route_object);
        damage_function(lua, -2, "getObjectDefinition", glue_route_definition);
        damage_function(lua, -2, "setVelocity", glue_route_velocity);
        damage_lua.create_table(lua, 0, 2);
        for (uint32_t object = 0; object < 2; ++object) {
            museum_glue_value(lua, glue_route_input[3 + object]);
            damage_lua.set_field(lua, -2, museum_glue_names[object]);
        }
        damage_lua.set_field(lua, -2, "deadBlocks");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 4);
        static const char *const collider_names[] = {"eagle", "bird", "egg", "glue"};
        static const uint32_t colliders[] = {1, 3, 6, 10};
        for (uint32_t collider = 0; collider < 4; ++collider) {
            damage_lua.push_number(lua, (float)colliders[collider]);
            damage_lua.set_field(lua, -2, collider_names[collider]);
        }
        damage_lua.set_field(lua, -2, "collider_types");
        damage_lua.set_field(lua, -2, "blockTable");
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_boolean(lua, (int)glue_route_input[5]);
        damage_lua.set_field(lua, -2, "shouldDisableContact");
        if (glue_route_input[5]) {
            void (*set_index)(void *, int, int) = (void (*)(
                void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
            damage_lua.create_table(lua, 2, 0);
            for (int index = 1; index <= 2; ++index) {
                damage_lua.push_string(lua, museum_glue_names[index - 1]);
                set_index(lua, -2, index);
            }
            damage_lua.set_field(lua, -2, "glueContactPair");
        }
        damage_lua.set_field(lua, -10002, "_ab_glue_route_response");
        damage_lua.get_field(lua, -10002, "MuseumCollisions");
        damage_lua.get_field(lua, 1, "onCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        for (int argument = 0; argument < 4; ++argument) {
            damage_lua.push_number(lua, 0);
        }
        damage_lua.get_field(lua, -10002, "_ab_glue_route_response");
        if (damage_lua.call(lua, 7, 0, 0)) {
            fail("Original enclosing glue route failed\n");
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_glue_route_response");
        damage_lua.get_field(lua, 1, "shouldDisableContact");
        uint32_t result[5] = {(uint32_t)damage_lua.type(lua, -1),
                              (uint32_t)(damage_lua.to_boolean(lua, -1) != 0), 0, 0, 0};
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "glueContactPair");
        result[2] = (uint32_t)damage_lua.type(lua, -1);
        if (result[2] == 5) {
            void (*get_index)(void *, int, int) = (void (*)(
                void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
            for (int index = 1; index <= 2; ++index) {
                get_index(lua, 2, index);
                result[index + 2] = museum_glue_name_id(lua, -1);
                damage_lua.set_top(lua, 2);
            }
        }
        print_words(result, 5);
        damage_lua.set_top(lua, 0);
        for (uint32_t object = 0; object < 3; ++object) {
            damage_lua.get_field(lua, -10002, museum_glue_names[object]);
            damage_lua.get_field(lua, 1, "name");
            uint32_t fields[7] = {museum_glue_name_id(lua, -1)};
            damage_lua.set_top(lua, 1);
            static const char *const field_names[] = {"grabber", "grabbedEgg",
                                                      "gluedToBlock"};
            for (uint32_t field = 0; field < 3; ++field) {
                damage_lua.get_field(lua, 1, field_names[field]);
                museum_glue_snapshot_value(lua, fields + 1 + field * 2);
                damage_lua.set_top(lua, 1);
            }
            print_words(fields, 7);
            damage_lua.set_top(lua, 0);
        }
        uint32_t dead[4];
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 1, "deadBlocks");
        for (uint32_t object = 0; object < 2; ++object) {
            damage_lua.get_field(lua, 2, museum_glue_names[object]);
            museum_glue_snapshot_value(lua, dead + object * 2);
            damage_lua.set_top(lua, 2);
        }
        print_words(dead, 4);
        damage_lua.set_top(lua, 0);
        print_words(&glue_route_count, 1);
        for (uint32_t event = 0; event < glue_route_count; ++event) {
            print_words(glue_route_events[event], 4);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB museum glue route reference complete\n");
}

#endif
