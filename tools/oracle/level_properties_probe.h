#ifndef AB_ORACLE_LEVEL_PROPERTIES_H
#define AB_ORACLE_LEVEL_PROPERTIES_H

#include "../../tests/level_properties_cases.h"
#include "object_property_graph_probe.h"

static uint32_t level_property_input[12], level_property_diagnostics;
static const char *const level_property_tables[] = {
    "_ab_level_physical", "_ab_level_script", "_ab_level_definition",
    "_ab_level_material", "_ab_level_blocks", "_ab_level_materials",
    "_ab_level_catalog",  "_ab_level_payload"};

static void level_native_scalar(void *lua, uint32_t kind) {
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind < 3)
        damage_lua.push_boolean(lua, kind == 2);
    else if (kind == 3) {
        FloatBits value = {.bits = level_property_input[8]};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 4)
        damage_lua.push_string(lua, "value");
    else
        damage_lua.get_field(lua, -10002, level_property_tables[7]);
}

static void level_native_record(void *lua, uint32_t kind, uint32_t role) {
    if (kind == 2)
        damage_lua.get_field(lua, -10002, level_property_tables[role]);
    else if (kind == 3)
        damage_lua.push_number(lua, 0.0f);
    else
        level_native_scalar(lua, kind == 4 ? 2u : kind == 5 ? 4u : kind);
}

static void level_native_name(void *lua) {
    static const char *const names[] = {"Bird_1", "stone_1", "\xc3\xa9\xe9\xb3\xa5",
                                        "A\0B"};
    static const uint32_t lengths[] = {6, 7, 5, 3};
    void (*push_bytes)(void *, const char *, unsigned) =
        (void (*)(void *, const char *, unsigned))find_function(
            "_Z15lua_pushlstringP9lua_StatePKcj");
    uint32_t index = level_property_input[11];
    push_bytes(lua, names[index], lengths[index]);
}

static int level_native_missing(void *lua) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u)
        fail("Missing original definition caller in property graph\n");
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0])
        fail("Invalid original definition closure in property graph\n");
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || closure[7] || !prototype || prototype[11] != 21 ||
        caller[3] != prototype[3] + 40u)
        fail("Unexpected original definition diagnostic caller in property graph\n");
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    if (!code || code[0] != UINT32_C(0x00000045) || code[20] != UINT32_C(0x0080001e))
        fail("Unexpected original definition instructions in property graph\n");
    static const char prefix[] = "Object not found:";
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 4)
        fail("Invalid original level property diagnostic arity/type\n");
    unsigned size = 0;
    const char *message = damage_lua.to_string(lua, 1, &size);
    for (uint32_t index = 0; index < sizeof(prefix) - 1u; ++index) {
        if (!message || index >= size || message[index] != prefix[index])
            fail("Invalid original level property diagnostic prefix\n");
    }
    if (level_property_input[6] == 5)
        damage_lua.push_string(lua, "absent");
    else
        level_native_name(lua);
    unsigned name_size = 0;
    const char *name = damage_lua.to_string(lua, 2, &name_size);
    if (size != sizeof(prefix) - 1u + name_size)
        fail("Invalid original level property diagnostic length\n");
    for (uint32_t index = 0; index < name_size; ++index) {
        if (message[sizeof(prefix) - 1u + index] != name[index])
            fail("Invalid original level property diagnostic name\n");
    }
    ++level_property_diagnostics;
    return 0;
}

static void level_native_graph(void *lua) {
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_object_property_graph(lua);
    damage_function(lua, -10002, "print", level_native_missing);
}

static void level_native_prepare(void *lua) {
    for (uint32_t role = 0; role < 8; ++role) {
        damage_lua.create_table(lua, 0, 6);
        damage_lua.set_field(lua, -10002, level_property_tables[role]);
    }
    damage_lua.get_field(lua, -10002, level_property_tables[0]);
    level_native_name(lua);
    damage_lua.set_field(lua, 1, "name");
    damage_lua.push_string(lua, "Bird");
    damage_lua.set_field(lua, 1, "definition");
    const char *const dimensions[] = {"width", "height", "radius"};
    const uint32_t words[] = {9, 10, 8};
    for (uint32_t index = 0; index < 3; ++index) {
        FloatBits value = {.bits = level_property_input[words[index]]};
        damage_lua.push_number(lua, value.number);
        damage_lua.set_field(lua, 1, dimensions[index]);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_level_init");
    damage_lua.get_field(lua, -10002, level_property_tables[1]);
    damage_lua.get_field(lua, -10002, level_property_tables[0]);
    if (damage_lua.call(lua, 2, 0, 0))
        fail("Original GameObject.init rejected valid physical metadata\n");
    damage_lua.get_field(lua, -10002, level_property_tables[0]);
    level_native_scalar(lua, level_property_input[1]);
    damage_lua.set_field(lua, 1, "radius");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[2]);
    level_native_scalar(lua, level_property_input[3]);
    damage_lua.set_field(lua, 1, "radius");
    damage_lua.push_string(lua, "wood");
    damage_lua.set_field(lua, 1, "materialName");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[3]);
    level_native_scalar(lua, level_property_input[5]);
    damage_lua.set_field(lua, 1, "radius");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[4]);
    level_native_record(lua, level_property_input[2], 2);
    damage_lua.set_field(lua, 1, "Bird");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[5]);
    level_native_record(lua, level_property_input[4], 3);
    damage_lua.set_field(lua, 1, "wood");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[6]);
    damage_lua.get_field(lua, -10002, level_property_tables[4]);
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.get_field(lua, -10002, level_property_tables[5]);
    damage_lua.set_field(lua, 1, "materials");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -10002, "blockTable");
    damage_lua.set_field(lua, 1, "blockTable");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    level_native_name(lua);
    damage_lua.get_field(lua, -10002, level_property_tables[0]);
    void (*raw_set)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    raw_set(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_level_world");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_level_world");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
}

static void level_native_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1)
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        if (!damage_named(lua, index, "value"))
            fail("Unexpected original level property string\n");
        output[1] = 1;
    } else if (output[0] == 5) {
        const void *selected = damage_lua.to_pointer(lua, index);
        int top = damage_lua.get_top(lua);
        damage_lua.get_field(lua, -10002, level_property_tables[7]);
        if (selected != damage_lua.to_pointer(lua, -1))
            fail("Unexpected original level property table\n");
        damage_lua.set_top(lua, top);
        output[1] = 1;
    } else if (output[0] != 0)
        fail("Unexpected original level property type\n");
}

static void level_native_get(void *lua, uint32_t output[3]) {
    damage_lua.get_field(lua, -10002, "_ab_level_get");
    damage_lua.get_field(lua, -10002, level_property_tables[1]);
    damage_lua.push_string(lua, "radius");
    output[0] = damage_lua.call(lua, 2, 1, 0) != 0;
    if (!output[0]) {
        level_native_value(lua, 1, output + 1);
    } else {
        const char *message = damage_lua.to_string(lua, 1, NULL);
        const char wanted[] = "attempt to index";
        int found = 0;
        for (uint32_t index = 0; message && message[index]; ++index) {
            uint32_t length = 0;
            while (wanted[length] && wanted[length] == message[index + length])
                ++length;
            found |= !wanted[length];
        }
        if (!found)
            fail(message ? message : "Unexpected original level property error\n");
    }
    damage_lua.set_top(lua, 0);
}

static void level_native_operation(void *lua) {
    uint32_t operation = level_property_input[6];
    if (operation == 1 || operation == 2) {
        damage_lua.get_field(lua, -10002, level_property_tables[0]);
        if (operation == 1)
            level_native_scalar(lua, level_property_input[7]);
        else
            damage_lua.push_string(lua, "value");
        damage_lua.set_field(lua, 1, operation == 1 ? "radius" : "name");
    } else if (operation == 3) {
        damage_lua.get_field(lua, -10002, "_ab_level_world");
        level_native_name(lua);
        damage_lua.push_nil(lua);
        void (*raw_set)(void *, int) =
            (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
        raw_set(lua, 1);
    } else if (operation == 4 || operation == 5) {
        damage_lua.get_field(lua, -10002, level_property_tables[1]);
        if (operation == 4)
            damage_lua.push_nil(lua);
        else
            damage_lua.push_string(lua, "absent");
        damage_lua.set_field(lua, 1, operation == 4 ? "physicsObject" : "name");
    }
    damage_lua.set_top(lua, 0);
}

static void level_native_snapshot(void *lua, uint32_t output[18]) {
    output[6] = level_property_diagnostics;
    damage_lua.get_field(lua, -10002, "_ab_level_world");
    level_native_name(lua);
    void (*raw_get)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_rawgetP9lua_Statei");
    raw_get(lua, 1);
    output[7] = damage_lua.type(lua, 2) == 5;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[1]);
    damage_lua.get_field(lua, 1, "physicsObject");
    output[8] = damage_lua.type(lua, 2) == 5;
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "name");
    output[9] = damage_named(lua, 2, "absent") != 0;
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "behaviors");
    output[16] = damage_lua.type(lua, 2) == 5;
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    damage_lua.push_nil(lua);
    output[17] = !next(lua, 2);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, level_property_tables[0]);
    damage_lua.get_field(lua, 1, "name");
    output[10] = damage_named(lua, 2, "value") != 0;
    damage_lua.set_top(lua, 1);
    const char *const dimensions[] = {"width", "height"};
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, 1, dimensions[index]);
        FloatBits value = {.number = damage_lua.to_number(lua, 2)};
        output[11 + index] = value.bits;
        damage_lua.set_top(lua, 1);
    }
    damage_lua.get_field(lua, 1, "radius");
    level_native_value(lua, 2, output + 13);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "gameObjectScript");
    const void *script = damage_lua.to_pointer(lua, 2);
    damage_lua.get_field(lua, -10002, level_property_tables[1]);
    output[15] = script == damage_lua.to_pointer(lua, 3);
    damage_lua.set_top(lua, 0);
}

static void probe_level_properties(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original level property state\n");
    level_native_graph(lua);
    semihost(4, "AB_ANDROID_LEVEL_PROPERTIES_V1 2048\n");
    for (uint32_t index = 0; index < LEVEL_PROPERTY_CASES; ++index) {
        level_property_case(index, level_property_input);
        print_words(level_property_input, 12);
        level_property_diagnostics = 0;
        level_native_prepare(lua);
        uint32_t output[18] = {0};
        level_native_get(lua, output);
        level_native_operation(lua);
        level_native_get(lua, output + 3);
        level_native_snapshot(lua, output);
        print_words(output, 18);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level properties reference complete\n");
}

#endif
