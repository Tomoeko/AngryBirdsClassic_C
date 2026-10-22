#ifndef AB_ORACLE_BLOCK_REMOVAL_SCORE_H
#define AB_ORACLE_BLOCK_REMOVAL_SCORE_H

#include "../../tests/block_removal_score_cases.h"

static uint32_t removal_score_input[25], removal_score_events[6][57];
static uint32_t removal_score_count, removal_score_calls[6], removal_score_cutoff;
static void (*removal_score_raw_get)(void *, int, int);
static const char *const removal_score_strings[] = {
    "body", "renamed", "glyph", "changed", "sound", "audio", "truthy"};
static const char *const removal_score_tables[] = {"_ab_score_object", "_ab_score_def1",
                                                   "_ab_score_def2", "_ab_score_value"};

static uint32_t removal_score_pc(void *lua) {
    const uint32_t *state = lua;
    uintptr_t ci = state[5], base = state[10];
    if (!ci || !base || ci < base + 24u || (ci - base) % 24u) {
        fail("Unexpected original removal scoring frame bounds\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original removal scoring caller\n");
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] || ((const uint8_t *)closure)[7] != 1 ||
        !closure[4]) {
        fail("Unexpected original removal scoring closure\n");
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 479 || !prototype[3]) {
        fail("Unexpected original removal scoring prototype\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (code[0] != UINT32_C(0x00000005) || code[478] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 479u * 4u || (saved - begin) % 4u) {
        fail("Unexpected original removal scoring instruction\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static void removal_score_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t string = 0; string < 7; ++string) {
            if (damage_named(lua, index, removal_score_strings[string])) {
                output[1] = string + 1u;
                return;
            }
        }
        fail("Unexpected original removal scoring string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, index, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] < 201 || output[1] > 204) {
            fail("Unexpected original removal scoring table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original removal scoring value\n");
    }
}

static void removal_score_scalar(void *lua, uint32_t kind, const char *string) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, string);
    } else {
        damage_lua.get_field(lua, -10002, removal_score_tables[3]);
    }
}

static void removal_score_set(void *lua, const char *table, const char *field,
                              uint32_t kind, const char *string) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, table);
    removal_score_scalar(lua, kind, string);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static void removal_score_number(void *lua, const char *table, const char *field,
                                 uint32_t present, uint32_t word) {
    int top = damage_lua.get_top(lua);
    if (table) {
        damage_lua.get_field(lua, -10002, table);
    }
    FloatBits value = {.bits = word};
    if (present) {
        damage_lua.push_number(lua, value.number);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, table ? top + 1 : -10002, field);
    damage_lua.set_top(lua, top);
}

static void removal_score_global_value(void *lua, const char *global,
                                       uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, global);
    removal_score_value(lua, top + 1, output);
    damage_lua.set_top(lua, top);
}

static int removal_score_audio_first(void *lua);
static int removal_score_audio_second(void *lua);

static uint32_t removal_score_audio_binding(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "playAudio");
    uint32_t binding = 0;
    if (damage_lua.type(lua, -1) == 6) {
        const uint32_t *closure = damage_lua.to_pointer(lua, -1);
        binding =
            closure[4] == (uint32_t)(uintptr_t)removal_score_audio_first ? 1u : 2u;
        if (binding == 2 &&
            closure[4] != (uint32_t)(uintptr_t)removal_score_audio_second) {
            fail("Unexpected original removal audio binding\n");
        }
    }
    damage_lua.set_top(lua, top);
    return binding;
}

static void removal_score_snapshot(void *lua, uint32_t output[37]) {
    int top = damage_lua.get_top(lua);
    const char *const fields[] = {"name", "levelGoal", "x", "y"};
    damage_lua.get_field(lua, -10002, removal_score_tables[0]);
    for (uint32_t field = 0; field < 4; ++field) {
        damage_lua.get_field(lua, top + 1, fields[field]);
        removal_score_value(lua, top + 2, output + field * 2u);
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    for (uint32_t definition = 0; definition < 2; ++definition) {
        damage_lua.get_field(lua, -10002, removal_score_tables[definition + 1u]);
        const char *const names[] = {"spriteScore", "damageSound"};
        for (uint32_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, top + 1, names[field]);
            removal_score_value(lua, top + 2,
                                output + 8 + definition * 4u + field * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
    removal_score_global_value(lua, "_ab_score_definition", output + 16);
    removal_score_global_value(lua, "_ab_score_increment", output + 18);
    removal_score_global_value(lua, "floatingScoreScaling", output + 20);
    damage_lua.get_field(lua, -10002, "_ab_score_blocks");
    damage_lua.get_field(lua, top + 1, "score");
    removal_score_value(lua, top + 2, output + 22);
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, top + 1, "individualDestructionScores");
    output[24] = damage_lua.type(lua, -1) == 5;
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "_ab_score_array");
    removal_score_raw_get(lua, top + 1, 1);
    output[25] = damage_lua.type(lua, -1) != 0;
    FloatBits first = {.number = damage_lua.to_number(lua, -1)};
    output[26] = first.bits;
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "scoreTable");
    damage_lua.get_field(lua, top + 1, "blocks");
    output[27] = damage_lua.type(lua, -1) == 5;
    damage_lua.set_top(lua, top);
    output[28] = removal_score_audio_binding(lua);
    output[29] = 0;
    const char *const hosts[] = {"getObjectDefinition",
                                 "g_get_score_to_add_for_breaking", "getAudioName",
                                 "addFloatingScore"};
    for (uint32_t host = 0; host < 4; ++host) {
        damage_lua.get_field(lua, -10002, hosts[host]);
        if (damage_lua.type(lua, -1) == 6) {
            output[29] |= 1u << host;
        }
        damage_lua.set_top(lua, top);
    }
    output[30] = removal_score_input[23];
    for (uint32_t kind = 0; kind < 6; ++kind) {
        output[31 + kind] = removal_score_calls[kind];
    }
}

static uint32_t *removal_score_record(void *lua, uint32_t kind, uint32_t pc,
                                      uint32_t variant, uint32_t argc) {
    uint32_t actual = removal_score_pc(lua);
    if ((pc && actual != pc) ||
        (!pc && actual != 82 && actual != 103 && actual != 123 && actual != 144) ||
        damage_lua.get_top(lua) != (int)argc || removal_score_count >= 6) {
        fail("Unexpected original removal scoring boundary\n");
    }
    uint32_t *event = removal_score_events[removal_score_count++];
    for (uint32_t word = 0; word < 57; ++word) {
        event[word] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    removal_score_snapshot(lua, event + 20);
    ++removal_score_calls[kind - 1u];
    return event;
}

static int removal_score_destroy(void *lua) {
    uint32_t *event = removal_score_record(lua, 1, 45, 0, 1);
    damage_lua.get_field(lua, 1, "_object");
    uint32_t object[2];
    removal_score_value(lua, 2, object);
    if (object[0] != 5 || object[1] != 201) {
        fail("Original removal destroy lost cached receiver\n");
    }
    damage_lua.set_top(lua, 1);
    event[4] = 501;
    uint32_t mutation = removal_score_input[16];
    if (mutation & 1u) {
        removal_score_set(lua, removal_score_tables[0], "name", 4, "renamed");
    }
    if (mutation & 2u) {
        removal_score_set(lua, removal_score_tables[0], "levelGoal", 1, "truthy");
    }
    if (mutation & 4u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "g_get_score_to_add_for_breaking");
    }
    if (mutation & 8u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "getObjectDefinition");
    }
    if (mutation & 16u) {
        removal_score_set(lua, removal_score_tables[1], "spriteScore", 4, "changed");
    }
    if (mutation & 32u) {
        damage_lua.get_field(lua, -10002, removal_score_tables[2]);
        damage_lua.set_field(lua, -10002, "_ab_score_definition");
    }
    if (mutation & 64u) {
        removal_score_set(lua, removal_score_tables[0], "x", 2, "truthy");
    }
    if (mutation & 128u) {
        removal_score_number(lua, NULL, "floatingScoreScaling", 1,
                             removal_score_input[20]);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_score_definition(void *lua) {
    uint32_t *event = removal_score_record(lua, 2, 48, 0, 1);
    if (!damage_named(lua, 1, "queued")) {
        fail("Original removal definition lost captured key\n");
    }
    event[4] = 1;
    if (removal_score_input[16] & 256u) {
        removal_score_number(lua, NULL, "_ab_score_increment", 0, 0);
    }
    if (removal_score_input[16] & 512u) {
        removal_score_set(lua, removal_score_tables[0], "levelGoal", 2, "truthy");
    }
    damage_lua.get_field(lua, -10002, "_ab_score_definition");
    damage_lua.set_field(lua, -10002, "_ab_score_returned_definition");
    damage_lua.get_field(lua, -10002, "_ab_score_returned_definition");
    return 1;
}

static int removal_score_breaking(void *lua) {
    uint32_t *event = removal_score_record(lua, 3, 51, 0, 1);
    uint32_t object[2];
    removal_score_value(lua, 1, object);
    if (object[0] != 5 || object[1] != 201) {
        fail("Original breaking score lost cached object\n");
    }
    event[4] = 201;
    uint32_t mutation = removal_score_input[24];
    if (mutation & 1u) {
        removal_score_set(lua, removal_score_tables[0], "levelGoal", 1, "truthy");
    }
    if (mutation & 2u) {
        removal_score_set(lua, removal_score_tables[0], "levelGoal", 2, "truthy");
    }
    if (mutation & 4u) {
        removal_score_set(lua, removal_score_tables[1], "damageSound", 1, "truthy");
    }
    if (mutation & 8u) {
        removal_score_set(lua, removal_score_tables[1], "spriteScore", 0, "glyph");
    }
    if (mutation & 16u) {
        removal_score_set(lua, removal_score_tables[2], "spriteScore", 4, "changed");
    }
    if (mutation & 32u) {
        removal_score_number(lua, NULL, "floatingScoreScaling", 1,
                             removal_score_input[20]);
    }
    if (mutation & 64u) {
        removal_score_number(lua, "_ab_score_blocks", "score", 1,
                             removal_score_input[21]);
    }
    if (mutation & 128u) {
        damage_lua.get_field(lua, -10002, removal_score_tables[2]);
        damage_lua.set_field(lua, -10002, "_ab_score_definition");
    }
    damage_lua.get_field(lua, -10002, "_ab_score_increment");
    damage_lua.set_field(lua, -10002, "_ab_score_returned_increment");
    damage_lua.get_field(lua, -10002, "_ab_score_returned_increment");
    return 1;
}

static int removal_score_audio_name(void *lua) {
    uint32_t *event = removal_score_record(lua, 4, 61, 0, 1);
    removal_score_value(lua, 1, event + 4);
    uint32_t mutation = removal_score_input[17];
    if (mutation & 1u) {
        damage_function(lua, -10002, "playAudio", removal_score_audio_second);
    }
    if (mutation & 2u) {
        removal_score_set(lua, removal_score_tables[0], "x", 5, "truthy");
        removal_score_set(lua, removal_score_tables[0], "y", 1, "truthy");
    }
    if (mutation & 4u) {
        removal_score_set(lua, "_ab_score_returned_definition", "spriteScore", 4,
                          "changed");
    }
    if (mutation & 8u) {
        removal_score_number(lua, NULL, "floatingScoreScaling", 0, 0);
    }
    if (mutation & 16u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "addFloatingScore");
    }
    if (mutation & 32u) {
        removal_score_number(lua, "_ab_score_blocks", "score", 1,
                             removal_score_input[21]);
    }
    if (mutation & 64u) {
        removal_score_set(lua, removal_score_tables[0], "levelGoal", 1, "truthy");
    }
    if (mutation & 128u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "playAudio");
    }
    removal_score_scalar(lua, removal_score_input[14], "audio");
    return 1;
}

static int removal_score_audio(void *lua, uint32_t variant) {
    uint32_t *event = removal_score_record(lua, 5, 64, variant, 3);
    for (int argument = 1; argument <= 4; ++argument) {
        if (argument <= 3) {
            removal_score_value(lua, argument, event + 4 + (argument - 1) * 2);
        }
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_score_audio_first(void *lua) {
    return removal_score_audio(lua, 1);
}

static int removal_score_audio_second(void *lua) {
    return removal_score_audio(lua, 2);
}

static int removal_score_floating(void *lua) {
    uint32_t *event = removal_score_record(lua, 6, 0, 0, 1);
    if (damage_lua.type(lua, 1) != 5) {
        fail("Original removal floating metadata is not a table\n");
    }
    const char *const values[] = {"x", "y", "sprite"};
    for (uint32_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, 1, values[field]);
        removal_score_value(lua, 2, event + 4 + field * 2u);
        damage_lua.set_top(lua, 1);
    }
    damage_lua.get_field(lua, 1, "text");
    event[10] = (uint32_t)damage_lua.type(lua, 2);
    if (event[10] == 4) {
        unsigned length = 0;
        const char *text = damage_lua.to_string(lua, 2, &length);
        event[11] = UINT32_C(2166136261);
        event[12] = length;
        for (unsigned index = 0; index < length; ++index) {
            event[11] = (event[11] ^ (uint8_t)text[index]) * UINT32_C(16777619);
        }
    } else if (event[10]) {
        fail("Unexpected original removal score text\n");
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "score");
    removal_score_value(lua, 2, event + 13);
    damage_lua.set_top(lua, 1);
    const char *const numbers[] = {"time", "lifetime", "maxScale", "xs"};
    for (uint32_t field = 0; field < 4; ++field) {
        damage_lua.get_field(lua, 1, numbers[field]);
        if (damage_lua.type(lua, 2) != 3) {
            fail("Unexpected original removal floating number\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, 2)};
        event[15 + field] = value.bits;
        damage_lua.set_top(lua, 1);
    }
    uint32_t mutation = removal_score_input[18];
    if (mutation & 1u) {
        removal_score_number(lua, "_ab_score_blocks", "score", 1,
                             removal_score_input[21]);
    }
    if (mutation & 2u) {
        removal_score_number(lua, "_ab_score_blocks", "score", 0, 0);
    }
    if (mutation & 4u) {
        removal_score_set(lua, "_ab_score_blocks", "individualDestructionScores", 0,
                          "");
    }
    if (mutation & 8u) {
        removal_score_set(lua, "scoreTable", "blocks", 0, "");
    }
    if (mutation & 16u) {
        removal_score_set(lua, removal_score_tables[0], "name", 4, "renamed");
    }
    if (mutation & 32u) {
        removal_score_number(lua, NULL, "floatingScoreScaling", 1,
                             removal_score_input[20]);
    }
    if (mutation & 64u) {
        removal_score_set(lua, removal_score_tables[1], "spriteScore", 4, "changed");
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_score_suffix(void *lua) {
    if (removal_score_pc(lua) != 164 || damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 2, "particles") || removal_score_cutoff++) {
        fail("Unexpected original post-score cutoff\n");
    }
    damage_lua.get_field(lua, 1, "_object");
    uint32_t object[2];
    removal_score_value(lua, 3, object);
    if (object[0] != 5 || object[1] != 201) {
        fail("Original post-score cutoff lost cached receiver\n");
    }
    damage_lua.set_top(lua, 2);
    damage_lua.push_nil(lua);
    return 1;
}

static int removal_score_contains(const char *message, const char *wanted) {
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

static void removal_score_prepare(void *lua) {
    for (uint32_t table = 0; table < 4; ++table) {
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_number(lua, (float)(201u + table));
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, removal_score_tables[table]);
    }
    removal_score_set(lua, removal_score_tables[0], "name", 4, "body");
    removal_score_set(lua, removal_score_tables[0], "levelGoal", removal_score_input[1],
                      "truthy");
    removal_score_set(lua, removal_score_tables[0], "x", removal_score_input[12],
                      "truthy");
    removal_score_set(lua, removal_score_tables[0], "y", removal_score_input[13],
                      "truthy");
    removal_score_number(lua, removal_score_tables[0], "strength", 1, 0);
    for (uint32_t table = 1; table <= 2; ++table) {
        removal_score_set(lua, removal_score_tables[table], "spriteScore",
                          removal_score_input[3], "glyph");
        removal_score_set(lua, removal_score_tables[table], "damageSound",
                          removal_score_input[4], "sound");
    }
    if (removal_score_input[2] >= 5) {
        damage_lua.get_field(lua, -10002,
                             removal_score_tables[removal_score_input[2] - 4u]);
    } else {
        removal_score_scalar(lua, removal_score_input[2], "truthy");
    }
    damage_lua.set_field(lua, -10002, "_ab_score_definition");
    removal_score_number(lua, NULL, "_ab_score_increment", removal_score_input[5],
                         removal_score_input[6]);
    removal_score_number(lua, NULL, "floatingScoreScaling", removal_score_input[7],
                         removal_score_input[8]);
    damage_lua.create_table(lua, 0, 3);
    damage_lua.get_field(lua, -10002, removal_score_tables[0]);
    damage_lua.set_field(lua, 1, "_object");
    if (removal_score_input[23] == 3) {
        damage_function(lua, 1, "onDestroy", removal_score_destroy);
    } else {
        removal_score_scalar(
            lua, removal_score_input[23] == 2 ? 3u : removal_score_input[23], "");
        damage_lua.set_field(lua, 1, "onDestroy");
    }
    damage_function(lua, 1, "get", removal_score_suffix);
    /* The script owns the cached object reference, independently of its name. */
    damage_lua.set_field(lua, -10002, "_ab_score_script");
    damage_lua.get_field(lua, -10002, removal_score_tables[0]);
    damage_lua.get_field(lua, -10002, "_ab_score_script");
    damage_lua.set_field(lua, 1, "gameObjectScript");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, removal_score_tables[0]);
    damage_lua.set_field(lua, 1, "queued");
    damage_lua.set_field(lua, -10002, "deadBlocks");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "deadBlocks");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 1, 0);
    damage_lua.set_field(lua, -10002, "_ab_score_array");
    damage_lua.create_table(lua, 0, 2);
    if (removal_score_input[11]) {
        damage_lua.get_field(lua, -10002, "_ab_score_array");
        damage_lua.set_field(lua, 1, "individualDestructionScores");
    }
    damage_lua.set_field(lua, -10002, "_ab_score_blocks");
    removal_score_number(lua, "_ab_score_blocks", "score", removal_score_input[9],
                         removal_score_input[10]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_score_blocks");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "scoreTable");
    const char *const names[] = {"getObjectDefinition",
                                 "g_get_score_to_add_for_breaking", "getAudioName",
                                 "addFloatingScore"};
    int (*const hosts[])(void *) = {removal_score_definition, removal_score_breaking,
                                    removal_score_audio_name, removal_score_floating};
    for (uint32_t host = 0; host < 4; ++host) {
        if (removal_score_input[22] & (1u << host)) {
            damage_function(lua, -10002, names[host], hosts[host]);
        } else {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, names[host]);
        }
    }
    if (removal_score_input[15]) {
        damage_function(lua, -10002, "playAudio",
                        removal_score_input[15] == 1 ? removal_score_audio_first
                                                     : removal_score_audio_second);
    } else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "playAudio");
    }
}

static void probe_block_removal_score(void) {
    initialize_damage_lua();
    removal_score_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal scoring state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    char chunk[3359];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk) ||
        system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, chunk, sizeof(chunk), "original-removeBlocks")) {
        fail("Cannot load original removal scoring prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_score_removeBlocks");
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_SCORE_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_removal_score_case(sample, removal_score_input);
        print_words(removal_score_input, 25);
        removal_score_count = removal_score_cutoff = 0;
        for (uint32_t kind = 0; kind < 6; ++kind) {
            removal_score_calls[kind] = 0;
        }
        removal_score_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_score_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        if (!status) {
            fail("Original removal scoring bypassed its cutoff\n");
        }
        const char *message = damage_lua.to_string(lua, -1, NULL);
        if ((removal_score_cutoff && (removal_score_cutoff != 1 ||
                                      !removal_score_contains(message, "index"))) ||
            (!removal_score_cutoff && !removal_score_contains(message, "index") &&
             !removal_score_contains(message, "call") &&
             !removal_score_contains(message, "concatenate") &&
             !removal_score_contains(message, "arithmetic") &&
             !removal_score_contains(message, "table expected"))) {
            fail(message ? message : "Unexpected original removal scoring error\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[5] = {removal_score_cutoff == 0, 0, 0, 0, 0};
        if (removal_score_cutoff) {
            removal_score_global_value(lua, "_ab_score_returned_definition",
                                       result + 1);
            removal_score_global_value(lua, "_ab_score_returned_increment", result + 3);
        }
        print_words(result, 5);
        uint32_t state[37];
        removal_score_snapshot(lua, state);
        print_words(state, 37);
        print_words(&removal_score_count, 1);
        for (uint32_t event = 0; event < removal_score_count; ++event) {
            print_words(removal_score_events[event], 57);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal score reference complete\n");
}

#endif
