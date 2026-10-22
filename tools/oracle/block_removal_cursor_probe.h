#ifndef AB_ORACLE_BLOCK_REMOVAL_CURSOR_H
#define AB_ORACLE_BLOCK_REMOVAL_CURSOR_H

#include "../../tests/block_removal_cursor_cases.h"

static uint32_t cursor_input[16], cursor_events[6][8], cursor_count;
static uint32_t cursor_selected[3], cursor_cutoff;
static const char *const cursor_names[] = {"Amber", "Pine",   "Bruno",
                                           "Lucid", "Chrome", "Zinc"};
static const char *const cursor_objects[] = {"_ab_cursor_Amber",  "_ab_cursor_Pine",
                                             "_ab_cursor_Bruno",  "_ab_cursor_Lucid",
                                             "_ab_cursor_Chrome", "_ab_cursor_Zinc"};
static const char *const cursor_maps[] = {"_ab_cursor_mapA", "_ab_cursor_mapB"};

static void cursor_scalar(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, "truthy");
    }
}

static void cursor_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t index = 0; index < 6; ++index) {
            if (damage_named(lua, -1, cursor_names[index])) {
                output[1] = index + 1u;
                return;
            }
        }
        if (!damage_named(lua, -1, "truthy")) {
            fail("Unexpected original removal traversal string\n");
        }
        output[1] = 7;
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] < 201 || output[1] > 209) {
            fail("Unexpected original removal traversal table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original removal traversal value\n");
    }
}

static uint32_t cursor_map_id(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "deadBlocks");
    const void *current = damage_lua.to_pointer(lua, -1);
    if (damage_lua.type(lua, -1) == 0) {
        damage_lua.set_top(lua, top);
        return 0;
    }
    for (uint32_t map = 0; map < 2; ++map) {
        damage_lua.get_field(lua, -10002, cursor_maps[map]);
        const void *candidate = damage_lua.to_pointer(lua, -1);
        damage_lua.set_top(lua, top + 1);
        if (candidate == current) {
            damage_lua.set_top(lua, top);
            return map + 1u;
        }
    }
    fail("Unexpected original current deadBlocks binding\n");
    return 0;
}

static uint32_t cursor_mask(void *lua, const char *map) {
    int top = damage_lua.get_top(lua);
    uint32_t mask = 0;
    damage_lua.get_field(lua, -10002, map);
    for (uint32_t index = 0; index < 6; ++index) {
        damage_lua.get_field(lua, top + 1, cursor_names[index]);
        if (damage_lua.type(lua, -1) != 0) {
            mask |= 1u << index;
        }
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    return mask;
}

static void cursor_snapshot(void *lua, uint32_t output[77]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "objects");
    cursor_value(lua, output);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "_ab_cursor_container");
    damage_lua.get_field(lua, top + 1, "world");
    cursor_value(lua, output + 2);
    damage_lua.set_top(lua, top);
    output[4] = cursor_map_id(lua);
    for (size_t map = 0; map < 2; ++map) {
        damage_lua.get_field(lua, -10002, cursor_maps[map]);
        for (size_t index = 0; index < 6; ++index) {
            damage_lua.get_field(lua, top + 1, cursor_names[index]);
            cursor_value(lua, output + 5 + map * 12u + index * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
    const char *const fields[] = {"strength", "rocket", "deathTimerFull", "name"};
    for (size_t index = 0; index < 6; ++index) {
        damage_lua.get_field(lua, -10002, cursor_objects[index]);
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, top + 1, fields[field]);
            cursor_value(lua, output + 29 + index * 8u + field * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
}

static uint32_t cursor_caller_pc(void *lua) {
    const uint32_t *state = lua;
    uintptr_t ci = state[5], base = state[10];
    if (!ci || !base || ci < base + 24u || (ci - base) % 24u) {
        fail("Unexpected original removeBlocks frame bounds\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original removeBlocks caller function\n");
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] || ((const uint8_t *)closure)[7] != 1 ||
        !closure[4]) {
        fail("Unexpected original removeBlocks closure\n");
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 479 || !prototype[3]) {
        fail("Unexpected original removeBlocks prototype\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (code[0] != UINT32_C(0x00000005) || code[478] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 479u * 4u || (saved - begin) % 4u) {
        fail("Unexpected original removeBlocks saved instruction\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static int cursor_rocket(void *lua) {
    if (cursor_caller_pc(lua) != 34 || damage_lua.get_top(lua) != 1 ||
        cursor_count >= 6) {
        fail("Unexpected original shootRocket boundary\n");
    }
    uint32_t *event = cursor_events[cursor_count];
    cursor_value(lua, event);
    event[2] = cursor_map_id(lua);
    event[3] = cursor_mask(lua, cursor_maps[0]);
    event[4] = cursor_mask(lua, cursor_maps[1]);
    damage_lua.get_field(lua, -10002, "objects");
    event[5] = (uint32_t)damage_lua.type(lua, -1);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, -10002, "_ab_cursor_container");
    damage_lua.get_field(lua, 2, "world");
    event[6] = (uint32_t)damage_lua.type(lua, -1);
    damage_lua.set_top(lua, 1);
    event[7] = cursor_count++;
    uint32_t mutation = cursor_input[7];
    if (mutation & 1u) {
        damage_lua.get_field(lua, -10002, cursor_maps[1]);
        damage_lua.set_field(lua, -10002, "deadBlocks");
    }
    if (mutation & 2u) {
        damage_lua.get_field(lua, -10002, "_ab_cursor_container");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 2, "world");
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 4u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "deadBlocks");
    }
    if (mutation & 8u) {
        damage_lua.get_field(lua, -10002, "_ab_cursor_world");
        for (size_t index = 0; index < 6; ++index) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 2, cursor_names[index]);
        }
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 16u) {
        damage_lua.get_field(lua, -10002, cursor_objects[5]);
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 2, "strength");
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 32u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -10002, "objects");
    }
    if (mutation & 64u) {
        damage_lua.get_field(lua, -10002, cursor_maps[0]);
        damage_lua.get_field(lua, 2, cursor_names[0]);
        if (damage_lua.type(lua, -1) != 0) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 2, cursor_names[0]);
        }
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 128u) {
        uint32_t current = cursor_map_id(lua);
        damage_lua.get_field(lua, -10002, cursor_maps[current == 1 ? 1u : 0u]);
        damage_lua.set_field(lua, -10002, "deadBlocks");
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int cursor_destroy(void *lua) {
    if (cursor_caller_pc(lua) != 45 || damage_lua.get_top(lua) != 1 ||
        cursor_selected[0]) {
        fail("Unexpected original removal selection boundary\n");
    }
    damage_lua.get_field(lua, 1, "_key");
    cursor_selected[0] = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "_object");
    cursor_selected[1] = 5;
    cursor_selected[2] = (uint32_t)damage_lua.to_number(lua, -1);
    if (cursor_selected[0] < 1 || cursor_selected[0] > 6 ||
        cursor_selected[2] != cursor_selected[0] + 200u) {
        fail("Original removal selection lost cached dead object\n");
    }
    return 0;
}

static int cursor_definition_cutoff(void *lua) {
    if (cursor_caller_pc(lua) != 48 || damage_lua.get_top(lua) != 1 ||
        !cursor_selected[0] ||
        !damage_named(lua, 1, cursor_names[cursor_selected[0] - 1u]) ||
        cursor_cutoff++) {
        fail("Unexpected original post-selection definition cutoff\n");
    }
    damage_lua.push_nil(lua);
    return 1;
}

static void cursor_strength(void *lua, uint32_t kind) {
    if (kind <= 2) {
        cursor_scalar(lua, kind);
        return;
    }
    if (kind == 7) {
        damage_lua.push_string(lua, "truthy");
        return;
    }
    uint32_t flags = cursor_input[15];
    FloatBits value = {
        .bits = kind == 3   ? (flags & 1u ? UINT32_C(0x80000000) : 0u)
                : kind == 4 ? (flags & 2u ? UINT32_C(0xff800000) : UINT32_C(0xbf800000))
                : kind == 5 ? (flags & 4u ? UINT32_C(0x7f800000) : UINT32_C(0x3f800000))
                            : UINT32_C(0x7fc01234)};
    damage_lua.push_number(lua, value.number);
}

static void cursor_table(void *lua, uint32_t id, const char *global) {
    damage_lua.create_table(lua, 0, 8);
    damage_lua.push_number(lua, (float)id);
    damage_lua.set_field(lua, 1, "_id");
    damage_lua.set_field(lua, -10002, global);
}

static void cursor_prepare(void *lua) {
    for (uint32_t index = 0; index < 6; ++index) {
        cursor_table(lua, index + 201u, cursor_objects[index]);
        damage_lua.get_field(lua, -10002, cursor_objects[index]);
        cursor_strength(lua, (cursor_input[10] >> (index * 3u)) & 7u);
        damage_lua.set_field(lua, 1, "strength");
        cursor_scalar(lua, (cursor_input[11] >> (index * 2u)) & 3u);
        damage_lua.set_field(lua, 1, "rocket");
        cursor_scalar(lua, (cursor_input[12] >> (index * 2u)) & 3u);
        damage_lua.set_field(lua, 1, "deathTimerFull");
        uint32_t name = (cursor_input[13] >> (index * 2u)) & 3u;
        if (!name) {
            damage_lua.push_string(lua, cursor_names[index]);
        } else {
            cursor_scalar(lua, name == 1 ? 0u : name == 2 ? 1u : 3u);
        }
        damage_lua.set_field(lua, 1, "name");
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_number(lua, (float)index + 1.0f);
        damage_lua.set_field(lua, 2, "_key");
        damage_lua.push_number(lua, (float)index + 201.0f);
        damage_lua.set_field(lua, 2, "_object");
        damage_function(lua, 2, "onDestroy", cursor_destroy);
        damage_lua.set_field(lua, 1, "gameObjectScript");
        damage_lua.set_top(lua, 0);
    }
    cursor_table(lua, 207, "_ab_cursor_world");
    cursor_table(lua, 208, "_ab_cursor_container");
    cursor_table(lua, 209, "_ab_cursor_empty");
    damage_lua.get_field(lua, -10002, "_ab_cursor_world");
    for (uint32_t index = 0; index < 6; ++index) {
        uint32_t kind = (cursor_input[5] >> (index * 2u)) & 3u;
        if (kind == 3) {
            damage_lua.get_field(lua, -10002, cursor_objects[index]);
        } else {
            cursor_scalar(lua, kind == 2 ? 3u : kind);
        }
        damage_lua.set_field(lua, 1, cursor_names[index]);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_cursor_container");
    uint32_t world = cursor_input[2];
    if (world >= 5) {
        damage_lua.get_field(lua, -10002,
                             world == 6 ? "_ab_cursor_world" : "_ab_cursor_empty");
    } else {
        cursor_scalar(lua, world);
    }
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_top(lua, 0);
    uint32_t objects = cursor_input[1];
    if (objects >= 5) {
        damage_lua.get_field(
            lua, -10002, objects == 6 ? "_ab_cursor_container" : "_ab_cursor_empty");
    } else {
        cursor_scalar(lua, objects);
    }
    damage_lua.set_field(lua, -10002, "objects");
    for (size_t map = 0; map < 2; ++map) {
        damage_lua.create_table(lua, 0, (int)cursor_input[map ? 14 : 6]);
        uint32_t live = cursor_input[map ? 8 : 4];
        for (uint32_t index = 0; index < 6; ++index) {
            if (!(live & (1u << index))) {
                continue;
            }
            uint32_t kind = (cursor_input[9] >> (index * 2u)) & 3u;
            if (kind == 3) {
                damage_lua.get_field(lua, -10002, cursor_objects[index]);
            } else {
                cursor_scalar(lua, kind == 2 ? 3u : kind + 1u);
            }
            damage_lua.set_field(lua, 1, cursor_names[index]);
        }
        damage_lua.set_field(lua, -10002, cursor_maps[map]);
    }
    if (cursor_input[3]) {
        damage_lua.get_field(lua, -10002, cursor_maps[0]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "deadBlocks");
    damage_function(lua, -10002, "shootRocket", cursor_rocket);
    damage_function(lua, -10002, "getObjectDefinition", cursor_definition_cutoff);
}

static int cursor_contains(const char *message, const char *wanted) {
    for (size_t start = 0; message && message[start]; ++start) {
        size_t offset = 0;
        while (wanted[offset] && message[start + offset] &&
               wanted[offset] == message[start + offset]) {
            ++offset;
        }
        if (!wanted[offset]) {
            return 1;
        }
    }
    return 0;
}

static void probe_block_removal_cursor(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal traversal state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    char chunk[3359];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk) ||
        system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, chunk, sizeof(chunk), "original-removeBlocks")) {
        fail("Cannot load original removeBlocks prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_cursor_removeBlocks");
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_CURSOR_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_removal_cursor_case(sample, cursor_input);
        print_words(cursor_input, 16);
        cursor_count = cursor_cutoff = 0;
        cursor_selected[0] = cursor_selected[1] = cursor_selected[2] = 0;
        cursor_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_cursor_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        if (cursor_selected[0]) {
            if (!status || cursor_cutoff != 1 || !cursor_contains(message, "call")) {
                fail(message ? message
                             : "Original removal selection missed its cutoff\n");
            }
        } else if (status && !cursor_contains(message, "index") &&
                   !cursor_contains(message, "compare") &&
                   !cursor_contains(message, "pairs")) {
            fail(message ? message : "Unexpected original removal traversal error\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[5] = {status != 0 && !cursor_selected[0],
                              cursor_selected[0] != 0, cursor_selected[0],
                              cursor_selected[1], cursor_selected[2]};
        print_words(result, 5);
        uint32_t state[77];
        cursor_snapshot(lua, state);
        print_words(state, 77);
        print_words(&cursor_count, 1);
        for (uint32_t event = 0; event < cursor_count; ++event) {
            print_words(cursor_events[event], 8);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal cursor reference complete\n");
}

#endif
