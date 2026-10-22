#ifndef AB_ORACLE_OBJECT_DEFINITION_LOOKUP_H
#define AB_ORACLE_OBJECT_DEFINITION_LOOKUP_H

#include "../../tests/object_definition_lookup_cases.h"

static uint32_t definition_lookup_input[DEFINITION_LOOKUP_INPUT_WORDS];
static uint32_t definition_lookup_calls, definition_lookup_event[12];
static int (*definition_lookup_raise)(void *);
static const char *const definition_lookup_tables[] = {
    "_ab_definition_t0", "_ab_definition_t1", "_ab_definition_t2", "_ab_definition_t3",
    "_ab_definition_t4", "_ab_definition_t5", "_ab_definition_t6"};

static void definition_native_push(void *lua, uint32_t kind, uint32_t role) {
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind <= 2)
        damage_lua.push_boolean(lua, kind == 2);
    else if (kind == 3) {
        FloatBits value = {.bits = definition_lookup_input[role == 6 ? 11 : 10]};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 4)
        damage_lua.push_string(lua, "value");
    else
        damage_lua.get_field(lua, -10002, definition_lookup_tables[role]);
}

static void definition_native_name(void *lua) {
#if defined(AB_ORACLE_OBJECT_WORLD_LOOKUP)
    if (definition_lookup_input[0] % 2u == 0) {
        definition_native_push(lua, definition_lookup_input[6], 5);
        return;
    }
#endif
    DefinitionLookupName name = definition_lookup_name(definition_lookup_input[8]);
    void (*push_bytes)(void *, const char *, unsigned) =
        (void (*)(void *, const char *, unsigned))find_function(
            "_Z15lua_pushlstringP9lua_StatePKcj");
    push_bytes(lua, name.data, name.size);
}

static __attribute__((noinline)) int definition_record_missing(void *lua) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u || damage_lua.get_top(lua) != 1 ||
        damage_lua.type(lua, 1) != 4 || definition_lookup_calls)
        fail("Invalid original definition-lookup diagnostic boundary\n");
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0])
        fail("Missing original definition-lookup caller closure\n");
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || closure[7] || !prototype || prototype[11] != 21 ||
        !prototype[3] || caller[3] != prototype[3] + 40u)
        fail("Unexpected original definition-lookup diagnostic caller\n");
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    if (code[0] != UINT32_C(0x00000045) || code[20] != UINT32_C(0x0080001e))
        fail("Unexpected original definition-lookup instructions\n");
    definition_lookup_event[0] = ++definition_lookup_calls;
    definition_lookup_event[1] = 9;
    definition_lookup_event[2] = 21;
    definition_lookup_event[3] = 1;
    unsigned length = 0;
    const char *message = damage_lua.to_string(lua, 1, &length);
    if (!message || length > 28)
        fail("Original definition-lookup diagnostic exceeds capture bounds\n");
    definition_lookup_event[4] = length;
    for (uint32_t index = 0; index < length; ++index)
        definition_lookup_event[5 + index / 4u] |= (uint32_t)(uint8_t)message[index]
                                                   << (index % 4u * 8u);
    uint32_t expected[8];
    definition_lookup_message(definition_lookup_name(definition_lookup_input[8]),
                              expected);
    for (uint32_t index = 0; index < 8; ++index) {
        if (expected[index] != definition_lookup_event[4 + index])
            fail("Original definition-lookup diagnostic text differs\n");
    }
    if (definition_lookup_input[9] & 4u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -10002, "objects");
    }
    if (definition_lookup_input[9] & 2u) {
        damage_lua.push_string(lua, "lookup-host-rejected");
        return 1;
    }
    return 0;
}

/* The APK raises errors with C++ unwinding. Keep the rejecting call in a
 * tail-call boundary so it does not unwind through the freestanding C probe. */
static int definition_native_missing(void *lua) {
    if (definition_record_missing(lua))
        return definition_lookup_raise(lua);
    return 0;
}

static void definition_native_field(void *lua, uint32_t role, const char *key,
                                    uint32_t kind, uint32_t value_role) {
    damage_lua.get_field(lua, -10002, definition_lookup_tables[role]);
    definition_native_push(lua, kind, value_role);
    damage_lua.set_field(lua, 1, key);
    damage_lua.set_top(lua, 0);
}

static void definition_native_prepare(void *lua) {
    void (*raw_set)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    for (uint32_t index = 0; index < DEFINITION_LOOKUP_TABLES; ++index) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.set_field(lua, -10002, definition_lookup_tables[index]);
    }
    definition_native_field(lua, 0, "world", definition_lookup_input[2], 1);
#if defined(AB_ORACLE_OBJECT_WORLD_LOOKUP)
    if (definition_lookup_input[0] % 2u ||
        !definition_lookup_invalid_key(definition_lookup_input[6],
                                       definition_lookup_input[10]))
#endif
    {
        damage_lua.get_field(lua, -10002, definition_lookup_tables[1]);
        definition_native_name(lua);
        definition_native_push(lua, definition_lookup_input[3], 2);
        raw_set(lua, 1);
        damage_lua.set_top(lua, 0);
    }
    definition_native_field(lua, 2, "definition", definition_lookup_input[6], 5);
    definition_native_field(lua, 3, "blocks", definition_lookup_input[5], 4);
    if (definition_lookup_input[12] ||
        !definition_lookup_invalid_key(definition_lookup_input[6],
                                       definition_lookup_input[10])) {
        damage_lua.get_field(lua, -10002, definition_lookup_tables[4]);
        if (definition_lookup_input[12] == 1) {
            damage_lua.push_string(lua, "other");
        } else if (definition_lookup_input[12] == 2) {
            FloatBits key = {.bits =
                                 definition_lookup_input[10] ^ UINT32_C(0x80000000)};
            damage_lua.push_number(lua, key.number);
        } else if (definition_lookup_input[12] == 3) {
            definition_native_push(lua, 5, 6);
        } else {
            definition_native_push(lua, definition_lookup_input[6], 5);
        }
        definition_native_push(lua, definition_lookup_input[7], 6);
        raw_set(lua, 1);
        damage_lua.set_top(lua, 0);
    }
    definition_native_push(lua, definition_lookup_input[1], 0);
    damage_lua.set_field(lua, -10002, "objects");
    definition_native_push(lua, definition_lookup_input[4], 3);
    damage_lua.set_field(lua, -10002, "blockTable");
    if (definition_lookup_input[9] & 1u) {
        if (definition_lookup_input[9] & 8u)
            damage_lua.push_boolean(lua, 0);
        else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "print");
    } else {
        damage_function(lua, -10002, "print", definition_native_missing);
    }
}

static void definition_native_value(void *lua, int index, uint32_t output[2]) {
    int type = damage_lua.type(lua, index);
    output[0] = (uint32_t)type;
    output[1] = 0;
    if (type == 0)
        return;
    if (type == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (type == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (type == 4) {
        if (!damage_named(lua, index, "value"))
            fail("Unexpected original definition-lookup string result\n");
        output[1] = 1;
    } else if (type == 5) {
        int top = damage_lua.get_top(lua);
        const void *selected = damage_lua.to_pointer(lua, index);
        for (uint32_t role = 0; role < DEFINITION_LOOKUP_TABLES; ++role) {
            damage_lua.get_field(lua, -10002, definition_lookup_tables[role]);
            if (selected == damage_lua.to_pointer(lua, -1))
                output[1] = role + 1u;
            damage_lua.set_top(lua, top);
        }
        if (!output[1])
            fail("Unknown original definition-lookup table result\n");
    } else {
        fail("Unexpected original definition-lookup value type\n");
    }
}

static int definition_native_expected_error(const char *message) {
    const char *const allowed[] = {"index", "call", "lookup-host-rejected"};
    for (uint32_t word = 0; word < 3; ++word) {
        for (uint32_t start = 0; message && message[start]; ++start) {
            uint32_t offset = 0;
            while (allowed[word][offset] &&
                   message[start + offset] == allowed[word][offset])
                ++offset;
            if (!allowed[word][offset])
                return 1;
        }
    }
    return 0;
}

static void probe_object_definition_lookup(void) {
    initialize_damage_lua();
    definition_lookup_raise = (int (*)(void *))find_function("_Z9lua_errorP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original definition-lookup state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    char chunk[512];
#if defined(AB_ORACLE_OBJECT_WORLD_LOOKUP)
    const char *path = "/data/local/tmp/ab520/camera/get-object.lua";
#else
    const char *path = "/data/local/tmp/ab520/camera/get-object-definition.lua";
#endif
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, chunk, sizeof(chunk));
    if (size <= 0 || size == sizeof(chunk) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, chunk, (unsigned)size, "original-definition-lookup"))
        fail("Cannot load original definition-lookup method\n");
    damage_lua.set_field(lua, -10002, "_ab_definition_lookup");
#if defined(AB_ORACLE_OBJECT_WORLD_LOOKUP)
    semihost(4, "AB_ANDROID_OBJECT_WORLD_LOOKUP_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_OBJECT_DEFINITION_LOOKUP_V1 4096\n");
#endif
    for (uint32_t item = 0; item < DEFINITION_LOOKUP_CASES; ++item) {
        object_definition_lookup_case(item, definition_lookup_input);
        print_words(definition_lookup_input, DEFINITION_LOOKUP_INPUT_WORDS);
        definition_lookup_calls = 0;
        for (uint32_t index = 0; index < DEFINITION_LOOKUP_EVENT_WORDS; ++index)
            definition_lookup_event[index] = 0;
        definition_native_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_definition_lookup");
        definition_native_name(lua);
        uint32_t output[DEFINITION_LOOKUP_OUTPUT_WORDS] = {0};
        output[0] = damage_lua.call(lua, 1, 1, 0) != 0;
        if (output[0]) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (!definition_native_expected_error(message))
                fail(message ? message
                             : "Unexpected original definition-lookup error\n");
        } else {
            definition_native_value(lua, 1, output + 1);
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "objects");
        definition_native_value(lua, 1, output + 3);
        damage_lua.set_top(lua, 0);
        output[5] = definition_lookup_calls;
        print_words(output, DEFINITION_LOOKUP_OUTPUT_WORDS);
        if (definition_lookup_calls)
            print_words(definition_lookup_event, DEFINITION_LOOKUP_EVENT_WORDS);
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_OBJECT_WORLD_LOOKUP)
    semihost(4, "AB object world lookup reference complete\n");
#else
    semihost(4, "AB object definition lookup reference complete\n");
#endif
}

#endif
