#ifndef AB_ORACLE_BLOCK_REMOVAL_EFFECTS_H
#define AB_ORACLE_BLOCK_REMOVAL_EFFECTS_H

#include "../../tests/block_removal_effects_cases.h"

static uint32_t removal_effects_input[22], removal_effects_events[9][57];
static uint32_t removal_effects_count, removal_effects_calls[7], removal_effects_cutoff;
static uint32_t removal_effects_result[4];
static uintptr_t removal_effects_string_ids[12], removal_effects_table_ids[4];
static const char *const removal_effects_strings[] = {
    "body",      "renamed", "BOMB",    "other", "smokeBuff",    "batBuff",
    "flameBuff", "sound",   "changed", "audio", "tnt_explodes", "truthy"};
static const char *const removal_effects_tables[] = {
    "_ab_effects_object", "_ab_effects_replacement", "_ab_effects_definition",
    "_ab_effects_value"};
static const char *const removal_effects_maps[] = {"_ab_effects_mapA",
                                                   "_ab_effects_mapB"};

static uint32_t removal_effects_pc(void *lua) {
    const uint32_t *state = lua;
    uintptr_t ci = state[5], base = state[10];
    if (!ci || !base || ci < base + 24u || (ci - base) % 24u) {
        fail("Unexpected original removal effects frame bounds\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original removal effects caller\n");
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] || ((const uint8_t *)closure)[7] != 1 ||
        !closure[4]) {
        fail("Unexpected original removal effects closure\n");
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 479 || !prototype[3]) {
        fail("Unexpected original removal effects prototype\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (code[0] != UINT32_C(0x00000005) || code[478] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 479u * 4u || (saved - begin) % 4u) {
        fail("Unexpected original removal effects instruction\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static void removal_effects_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t string = 0; string < 12; ++string) {
            if (damage_named(lua, index, removal_effects_strings[string])) {
                output[1] = string + 1u;
                return;
            }
        }
        fail("Unexpected original removal effect string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, index, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] < 201 || output[1] > 204) {
            fail("Unexpected original removal effect table\n");
        }
    } else if (output[0]) {
        fail("Unexpected original removal effect value\n");
    }
}

/* Read-only TValue decoding, after caller/prototype/PC validation. Pinned
 * strings are identified from original API-created stack values, and tables
 * from original topointer; no TString layout or register writes are needed. */
static void removal_effects_register(const uint32_t value[2], uint32_t output[2]) {
    output[0] = value[1];
    output[1] = 0;
    if (value[1] == 1 || value[1] == 3) {
        output[1] = value[0];
        return;
    }
    const uintptr_t *identities =
        value[1] == 4 ? removal_effects_string_ids : removal_effects_table_ids;
    uint32_t count = value[1] == 4 ? 12u : 4u;
    if (!value[1]) {
        return;
    }
    if (value[1] != 4 && value[1] != 5) {
        fail("Unexpected original removal effect register tag\n");
    }
    for (uint32_t index = 0; index < count; ++index) {
        if (identities[index] == value[0]) {
            output[1] = index + (value[1] == 4 ? 1u : 201u);
            return;
        }
    }
    fail("Unexpected original removal effect register identity\n");
}

static void removal_effects_scalar(void *lua, uint32_t kind, const char *string) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, string);
    } else {
        damage_lua.get_field(lua, -10002, removal_effects_tables[3]);
    }
}

static void removal_effects_set(void *lua, const char *table, const char *field,
                                uint32_t kind, const char *string) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, table);
    removal_effects_scalar(lua, kind, string);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static void removal_effects_map(void *lua, uint32_t binding) {
    if (binding) {
        damage_lua.get_field(lua, -10002, removal_effects_maps[binding - 1u]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "deadBlocks");
}

static void removal_effects_clear(void *lua, const char *global) {
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, global);
}

static int removal_effects_audio_first(void *lua);
static int removal_effects_audio_second(void *lua);
static int removal_effects_explosion_first(void *lua);
static int removal_effects_explosion_second(void *lua);

static uint32_t removal_effects_binding(void *lua, const char *name,
                                        int (*first)(void *), int (*second)(void *)) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    uint32_t output = 0;
    if (damage_lua.type(lua, -1) == 6) {
        const uint32_t *closure = damage_lua.to_pointer(lua, -1);
        output = closure[4] == (uint32_t)(uintptr_t)first ? 1u : 2u;
        if (output == 2 && closure[4] != (uint32_t)(uintptr_t)second) {
            fail("Unexpected original removal effect binding\n");
        }
    }
    damage_lua.set_top(lua, top);
    return output;
}

static void removal_effects_snapshot(void *lua, uint32_t output[37]) {
    int top = damage_lua.get_top(lua);
    const char *const fields[] = {"name", "specialty", "explosionSound",
                                  "killSilently"};
    for (uint32_t object = 0; object < 2; ++object) {
        damage_lua.get_field(lua, -10002, removal_effects_tables[object]);
        for (uint32_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, top + 1, fields[field]);
            removal_effects_value(lua, top + 2, output + object * 8u + field * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, removal_effects_tables[2]);
    damage_lua.get_field(lua, top + 1, "specialty");
    removal_effects_value(lua, top + 2, output + 16);
    damage_lua.set_top(lua, top);
    const char *const globals[] = {"_ab_effects_particles", "_ab_effects_amount",
                                   "_ab_effects_sound"};
    for (uint32_t value = 0; value < 3; ++value) {
        damage_lua.get_field(lua, -10002, globals[value]);
        removal_effects_value(lua, top + 1, output + 18 + value * 2u);
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, "deadBlocks");
    const void *current = damage_lua.to_pointer(lua, -1);
    output[24] = 0;
    if (damage_lua.type(lua, -1)) {
        for (uint32_t map = 0; map < 2; ++map) {
            damage_lua.get_field(lua, -10002, removal_effects_maps[map]);
            if (damage_lua.to_pointer(lua, -1) == current) {
                output[24] = map + 1u;
            }
            damage_lua.set_top(lua, top + 1);
        }
        if (!output[24]) {
            fail("Unexpected original removal effect map\n");
        }
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, removal_effects_maps[0]);
    damage_lua.get_field(lua, top + 1, "queued");
    removal_effects_value(lua, top + 2, output + 25);
    damage_lua.set_top(lua, top);
    output[27] = removal_effects_binding(lua, "playAudio", removal_effects_audio_first,
                                         removal_effects_audio_second);
    output[28] =
        removal_effects_binding(lua, "makeExplosion", removal_effects_explosion_first,
                                removal_effects_explosion_second);
    output[29] = 0;
    damage_lua.get_field(lua, -10002, "_ab_effects_script");
    damage_lua.get_field(lua, top + 1, "get");
    if (damage_lua.type(lua, -1) == 6) {
        output[29] |= 1u;
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "g_levelEffect");
    damage_lua.get_field(lua, top + 1, "destroyedSound");
    if (damage_lua.type(lua, -1) == 6) {
        output[29] |= 2u;
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "math");
    damage_lua.get_field(lua, top + 1, "random");
    if (damage_lua.type(lua, -1) == 6) {
        output[29] |= 4u;
    }
    damage_lua.set_top(lua, top);
    output[29] |= output[28] ? 8u : 0u;
    const char *const names[] = {"getAudioName", "updateDestroyedBlocksValues"};
    for (uint32_t host = 0; host < 2; ++host) {
        damage_lua.get_field(lua, -10002, names[host]);
        if (damage_lua.type(lua, -1) == 6) {
            output[29] |= 16u << host;
        }
        damage_lua.set_top(lua, top);
    }
    for (uint32_t kind = 0; kind < 7; ++kind) {
        output[30 + kind] = removal_effects_calls[kind];
    }
}

static uint32_t *removal_effects_record(void *lua, uint32_t kind, uint32_t pc,
                                        uint32_t variant, uint32_t argc) {
    if (removal_effects_pc(lua) != pc || damage_lua.get_top(lua) != (int)argc ||
        removal_effects_count >= 9) {
        fail("Unexpected original removal effect boundary\n");
    }
    uint32_t *event = removal_effects_events[removal_effects_count++];
    for (uint32_t word = 0; word < 57; ++word) {
        event[word] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    removal_effects_snapshot(lua, event + 20);
    ++removal_effects_calls[kind - 1u];
    return event;
}

static void removal_effects_require_object(void *lua, int index, uint32_t wanted) {
    uint32_t value[2];
    removal_effects_value(lua, index, value);
    if (value[0] != 5 || value[1] != wanted) {
        fail("Original removal effect lost cached object/definition\n");
    }
}

static void removal_effects_clear_method(void *lua, const char *table,
                                         const char *field) {
    removal_effects_set(lua, table, field, 0, "");
}

static int removal_effects_property(void *lua) {
    int particles = damage_named(lua, 2, "particles");
    if (!particles && !damage_named(lua, 2, "particleAmount")) {
        fail("Unexpected original removal effect property\n");
    }
    uint32_t *event = removal_effects_record(lua, 1, particles ? 164u : 173u, 0, 2);
    damage_lua.get_field(lua, 1, "_object");
    removal_effects_require_object(lua, 3, 201);
    damage_lua.set_top(lua, 2);
    event[4] = 501;
    event[5] = particles ? 1u : 2u;
    if (particles) {
        uint32_t mutation = removal_effects_input[15];
        if (mutation & 1u) {
            removal_effects_map(lua, 2);
        }
        if (mutation & 2u) {
            removal_effects_set(lua, removal_effects_tables[2], "specialty", 4, "BOMB");
        }
        if (mutation & 4u) {
            removal_effects_set(lua, removal_effects_tables[0], "killSilently", 1, "");
        }
        if (mutation & 8u) {
            removal_effects_clear_method(lua, "g_levelEffect", "destroyedSound");
        }
        if (mutation & 16u) {
            removal_effects_clear_method(lua, "_ab_effects_script", "get");
        }
        if (mutation & 32u) {
            removal_effects_set(lua, removal_effects_tables[0], "name", 4, "renamed");
        }
    }
    damage_lua.get_field(lua, -10002,
                         particles ? "_ab_effects_particles" : "_ab_effects_amount");
    return 1;
}

static int removal_effects_destroyed_sound(void *lua) {
    uint32_t *event = removal_effects_record(lua, 2, 169, 0, 3);
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 601) {
        fail("Original destroyed sound lost level-effect receiver\n");
    }
    damage_lua.set_top(lua, 3);
    removal_effects_require_object(lua, 2, 201);
    removal_effects_require_object(lua, 3, 203);
    event[4] = 601;
    event[5] = 201;
    event[6] = 203;
    uint32_t mutation = removal_effects_input[16];
    if (mutation & 1u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -10002, "_ab_effects_amount");
    }
    if (mutation & 2u) {
        damage_lua.push_string(lua, "changed");
        damage_lua.set_field(lua, -10002, "_ab_effects_particles");
    }
    if (mutation & 4u) {
        removal_effects_set(lua, removal_effects_tables[2], "specialty", 4, "other");
    }
    if (mutation & 8u) {
        removal_effects_map(lua, 0);
    }
    if (mutation & 16u) {
        removal_effects_clear_method(lua, "math", "random");
    }
    if (mutation & 32u) {
        damage_function(lua, -10002, "playAudio", removal_effects_audio_second);
    }
    damage_lua.get_field(lua, -10002, "_ab_effects_sound");
    return 1;
}

static int removal_effects_random(void *lua) {
    uint32_t *event = removal_effects_record(lua, 3, 182, 0, 2);
    FloatBits lower = {.number = damage_lua.to_number(lua, 1)};
    FloatBits upper = {.number = damage_lua.to_number(lua, 2)};
    if (damage_lua.type(lua, 1) != 3 || damage_lua.type(lua, 2) != 3 ||
        lower.number != 12 || upper.number != 20) {
        fail("Original removal random bounds changed\n");
    }
    event[4] = lower.bits;
    event[5] = upper.bits;
    uint32_t mutation = removal_effects_input[17];
    if (mutation & 1u) {
        removal_effects_map(lua, 2);
    }
    if (mutation & 2u) {
        removal_effects_set(lua, removal_effects_tables[1], "specialty", 4, "BOMB");
    }
    if (mutation & 4u) {
        removal_effects_set(lua, removal_effects_tables[0], "specialty", 4, "BOMB");
    }
    if (mutation & 8u) {
        removal_effects_set(lua, removal_effects_tables[0], "killSilently", 2, "");
    }
    if (mutation & 16u) {
        removal_effects_clear_method(lua, removal_effects_maps[0], "queued");
    }
    if (mutation & 32u) {
        removal_effects_clear(lua, "makeExplosion");
    }
    if (mutation & 64u) {
        damage_lua.push_string(lua, "changed");
        damage_lua.set_field(lua, -10002, "_ab_effects_sound");
    }
    removal_effects_scalar(lua, removal_effects_input[8], "truthy");
    return 1;
}

static int removal_effects_audio_name(void *lua) {
    int explosion =
        damage_lua.type(lua, 1) == 4 && damage_named(lua, 1, "tnt_explodes");
    uint32_t *event = removal_effects_record(lua, 4, explosion ? 213u : 223u, 0, 1);
    removal_effects_value(lua, 1, event + 4);
    uint32_t mutation = removal_effects_input[18];
    if (mutation & 1u) {
        damage_function(lua, -10002, "playAudio", removal_effects_audio_second);
    }
    if (mutation & 2u) {
        damage_function(lua, -10002, "makeExplosion", removal_effects_explosion_second);
    }
    if (mutation & 4u) {
        removal_effects_map(lua, 2);
    }
    if (mutation & 8u) {
        removal_effects_map(lua, 0);
    }
    if (mutation & 16u) {
        removal_effects_set(lua, removal_effects_tables[0], "killSilently", 2, "");
    }
    removal_effects_scalar(lua, removal_effects_input[20], "audio");
    return 1;
}

static int removal_effects_explosion(void *lua, uint32_t variant) {
    uint32_t *event = removal_effects_record(lua, 5, 214, variant, 3);
    for (int argument = 1; argument <= 3; ++argument) {
        removal_effects_value(lua, argument, event + 4 + (argument - 1) * 2);
    }
    removal_effects_require_object(lua, 2, 203);
    uint32_t mutation = removal_effects_input[19];
    if (mutation & 1u) {
        removal_effects_set(lua, removal_effects_tables[0], "killSilently", 1, "");
    }
    if (mutation & 2u) {
        damage_lua.push_string(lua, "changed");
        damage_lua.set_field(lua, -10002, "_ab_effects_sound");
    }
    if (mutation & 4u) {
        removal_effects_clear(lua, "playAudio");
    }
    if (mutation & 8u) {
        removal_effects_clear(lua, "updateDestroyedBlocksValues");
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_effects_explosion_first(void *lua) {
    return removal_effects_explosion(lua, 1);
}

static int removal_effects_explosion_second(void *lua) {
    return removal_effects_explosion(lua, 2);
}

static int removal_effects_audio(void *lua, uint32_t variant) {
    uint32_t *event = removal_effects_record(lua, 6, 227, variant, 4);
    for (int argument = 1; argument <= 4; ++argument) {
        removal_effects_value(lua, argument, event + 4 + (argument - 1) * 2);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_effects_audio_first(void *lua) {
    return removal_effects_audio(lua, 1);
}

static int removal_effects_audio_second(void *lua) {
    return removal_effects_audio(lua, 2);
}

static int removal_effects_update(void *lua) {
    uint32_t *event = removal_effects_record(lua, 7, 230, 0, 1);
    removal_effects_require_object(lua, 1, 201);
    event[4] = 201;
    if (removal_effects_cutoff++) {
        fail("Repeated original post-effect cutoff\n");
    }
    const uint32_t *state = lua;
    const uint32_t *caller = (const uint32_t *)(uintptr_t)(state[5] - 24u);
    const uint32_t *registers = (const uint32_t *)(uintptr_t)caller[0];
    if (!registers || registers + 22 > (const uint32_t *)(uintptr_t)state[3]) {
        fail("Unexpected original removal cached-register bounds\n");
    }
    removal_effects_register(registers + 16, removal_effects_result);
    removal_effects_register(registers + 20, removal_effects_result + 2);
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_effects_noop(void *lua) {
    (void)lua;
    return 0;
}

static int removal_effects_definition(void *lua) {
    if (removal_effects_pc(lua) != 48 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "queued")) {
        fail("Unexpected original pre-effect definition lookup\n");
    }
    damage_lua.get_field(lua, -10002, removal_effects_tables[2]);
    return 1;
}

static int removal_effects_score(void *lua) {
    if (removal_effects_pc(lua) != 51 || damage_lua.get_top(lua) != 1) {
        fail("Unexpected original pre-effect breaking score\n");
    }
    removal_effects_require_object(lua, 1, 201);
    damage_lua.push_number(lua, 0);
    return 1;
}

static int removal_effects_floating(void *lua) {
    if (removal_effects_pc(lua) != 123 || damage_lua.get_top(lua) != 1) {
        fail("Unexpected original pre-effect floating request\n");
    }
    removal_effects_map(lua, removal_effects_input[11]);
    return 0;
}

static void removal_effects_function(void *lua, int table, const char *name,
                                     int (*function)(void *), uint32_t enabled) {
    if (enabled) {
        damage_function(lua, table, name, function);
    } else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, table, name);
    }
}

static void removal_effects_prepare(void *lua) {
    for (uint32_t table = 0; table < 4; ++table) {
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_number(lua, (float)(201u + table));
        damage_lua.set_field(lua, 1, "_id");
        removal_effects_table_ids[table] = (uintptr_t)damage_lua.to_pointer(lua, 1);
        damage_lua.set_field(lua, -10002, removal_effects_tables[table]);
    }
    for (uint32_t object = 0; object < 2; ++object) {
        removal_effects_set(lua, removal_effects_tables[object], "name", 4, "body");
        removal_effects_set(lua, removal_effects_tables[object], "specialty",
                            removal_effects_input[object + 1u], "BOMB");
        removal_effects_set(lua, removal_effects_tables[object], "explosionSound",
                            removal_effects_input[object + 9u], "sound");
        removal_effects_set(lua, removal_effects_tables[object], "killSilently",
                            removal_effects_input[4], "truthy");
    }
    removal_effects_set(lua, removal_effects_tables[2], "specialty",
                        removal_effects_input[3], "BOMB");
    removal_effects_set(lua, removal_effects_tables[2], "spriteScore", 1, "");
    removal_effects_set(lua, removal_effects_tables[0], "strength", 3, "");
    uint32_t particle = removal_effects_input[5];
    if (particle < 4 || particle == 7) {
        removal_effects_scalar(lua, particle == 7 ? 5u : particle, "");
    } else {
        damage_lua.push_string(lua,
                               removal_effects_strings[particle == 8 ? 3u : particle]);
    }
    damage_lua.set_field(lua, -10002, "_ab_effects_particles");
    removal_effects_scalar(lua, removal_effects_input[6], "truthy");
    damage_lua.set_field(lua, -10002, "_ab_effects_amount");
    removal_effects_scalar(lua, removal_effects_input[7], "sound");
    damage_lua.set_field(lua, -10002, "_ab_effects_sound");
    uint32_t mask = removal_effects_input[13];
    damage_lua.create_table(lua, 0, 3);
    damage_lua.get_field(lua, -10002, removal_effects_tables[0]);
    damage_lua.set_field(lua, 1, "_object");
    damage_function(lua, 1, "onDestroy", removal_effects_noop);
    removal_effects_function(lua, 1, "get", removal_effects_property, mask & 1u);
    damage_lua.set_field(lua, -10002, "_ab_effects_script");
    damage_lua.get_field(lua, -10002, removal_effects_tables[0]);
    damage_lua.get_field(lua, -10002, "_ab_effects_script");
    damage_lua.set_field(lua, 1, "gameObjectScript");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, 601);
    damage_lua.set_field(lua, 1, "_id");
    removal_effects_function(lua, 1, "destroyedSound", removal_effects_destroyed_sound,
                             mask & 2u);
    damage_lua.set_field(lua, -10002, "g_levelEffect");
    damage_lua.create_table(lua, 0, 1);
    removal_effects_function(lua, 1, "random", removal_effects_random, mask & 4u);
    damage_lua.set_field(lua, -10002, "math");
    removal_effects_function(lua, -10002, "makeExplosion",
                             removal_effects_input[21] == 1
                                 ? removal_effects_explosion_first
                                 : removal_effects_explosion_second,
                             mask & 8u);
    removal_effects_function(lua, -10002, "getAudioName", removal_effects_audio_name,
                             mask & 16u);
    removal_effects_function(lua, -10002, "updateDestroyedBlocksValues",
                             removal_effects_update, mask & 32u);
    removal_effects_function(lua, -10002, "playAudio",
                             removal_effects_input[14] == 1
                                 ? removal_effects_audio_first
                                 : removal_effects_audio_second,
                             removal_effects_input[14]);
    for (uint32_t map = 0; map < 2; ++map) {
        damage_lua.create_table(lua, 0, 1);
        uint32_t kind = map ? 5u : removal_effects_input[12];
        if (kind >= 4) {
            damage_lua.get_field(lua, -10002, removal_effects_tables[kind - 4u]);
        } else {
            removal_effects_scalar(lua, kind, "");
        }
        damage_lua.set_field(lua, 1, "queued");
        damage_lua.set_field(lua, -10002, removal_effects_maps[map]);
    }
    /* A separate initial iterator reaches scoring. Its floating boundary sets
     * the desired live deadBlocks binding before the phase under comparison. */
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, removal_effects_tables[0]);
    damage_lua.set_field(lua, 1, "queued");
    damage_lua.set_field(lua, -10002, "deadBlocks");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "deadBlocks");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "score", 0);
    damage_lua.create_table(lua, 1, 0);
    damage_lua.set_field(lua, 2, "individualDestructionScores");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "scoreTable");
    damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, -10002, "floatingScoreScaling");
    damage_function(lua, -10002, "getObjectDefinition", removal_effects_definition);
    damage_function(lua, -10002, "g_get_score_to_add_for_breaking",
                    removal_effects_score);
    damage_function(lua, -10002, "addFloatingScore", removal_effects_floating);
}

static int removal_effects_contains(const char *message, const char *wanted) {
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

static void probe_block_removal_effects(void) {
    initialize_damage_lua();
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal effect state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 12);
    for (uint32_t string = 0; string < 12; ++string) {
        damage_lua.push_string(lua, removal_effects_strings[string]);
        const uint32_t *state = lua;
        const uint32_t *top = (const uint32_t *)(uintptr_t)state[2];
        if (top[-1] != 4 || !damage_named(lua, 2, removal_effects_strings[string])) {
            fail("Unexpected original pinned string TValue\n");
        }
        removal_effects_string_ids[string] = top[-2];
        damage_lua.set_field(lua, 1, removal_effects_strings[string]);
    }
    damage_lua.set_field(lua, -10002, "_ab_effects_pinned_strings");
    char chunk[3359];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk) ||
        system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, chunk, sizeof(chunk), "original-removeBlocks")) {
        fail("Cannot load original removal effects prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_effects_removeBlocks");
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_EFFECTS_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_removal_effects_case(sample, removal_effects_input);
        print_words(removal_effects_input, 22);
        removal_effects_count = removal_effects_cutoff = 0;
        for (uint32_t kind = 0; kind < 7; ++kind) {
            removal_effects_calls[kind] = 0;
        }
        removal_effects_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_effects_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        if (!status ||
            (removal_effects_cutoff && (removal_effects_cutoff != 1 ||
                                        !removal_effects_contains(message, "upvalue") ||
                                        !removal_effects_contains(message, "call"))) ||
            (!removal_effects_cutoff && !removal_effects_contains(message, "index") &&
             !removal_effects_contains(message, "call"))) {
            fail(message ? message : "Original removal effects bypassed cutoff\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[5] = {removal_effects_cutoff == 0, 0, 0, 0, 0};
        if (removal_effects_cutoff) {
            for (uint32_t word = 0; word < 4; ++word) {
                result[word + 1u] = removal_effects_result[word];
            }
        }
        print_words(result, 5);
        uint32_t state[37];
        removal_effects_snapshot(lua, state);
        print_words(state, 37);
        print_words(&removal_effects_count, 1);
        for (uint32_t event = 0; event < removal_effects_count; ++event) {
            print_words(removal_effects_events[event], 57);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal effects reference complete\n");
}

#endif
