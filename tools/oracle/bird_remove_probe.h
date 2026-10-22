#ifndef AB_ORACLE_BIRD_REMOVE_H
#define AB_ORACLE_BIRD_REMOVE_H

#include "../../tests/bird_remove_cases.h"

static uint32_t bird_remove_native_input[BIRD_REMOVE_INPUT_WORDS];
static uint32_t bird_remove_native_events[BIRD_REMOVE_MAX_EVENTS]
                                         [BIRD_REMOVE_EVENT_WORDS];
static uint32_t bird_remove_native_count;
static void (*bird_remove_native_push)(void *, int);
static const char *const bird_remove_native_tables[] = {
    "_ab_bremove_bird",      "_ab_bremove_script",    "_ab_bremove_definition",
    "_ab_bremove_hats",      "_ab_bremove_hat_entry", "_ab_bremove_hat_body",
    "_ab_bremove_objects_a", "_ab_bremove_world_a",   "_ab_bremove_dynamic_a",
    "_ab_bremove_birds_a",   "_ab_bremove_others_a",  "_ab_bremove_objects_b",
    "_ab_bremove_world_b",   "_ab_bremove_dynamic_b", "_ab_bremove_birds_b",
    "_ab_bremove_others_b",  "_ab_bremove_particles", "_ab_bremove_camera",
    "_ab_bremove_effect"};
static const char *const bird_remove_native_strings[] = {
    "Bird",           "Renamed",      "Hat",       "Other",  "hook",    "removed",
    "bird_destroyed", "bubble_sound", "particles", "truthy", "resolved"};

static int bird_remove_native_audio_two(void *lua);

static void bird_remove_native_table(void *lua, BirdRemoveTable index) {
    damage_lua.get_field(lua, -10002, bird_remove_native_tables[index]);
}

static void bird_remove_native_scalar(void *lua, uint32_t kind) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, "removed");
    } else {
        bird_remove_native_table(lua, BIRD_REMOVE_PARTICLES);
    }
}

static void bird_remove_native_value(void *lua, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    output[0] = (uint32_t)damage_lua.type(lua, top);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, top) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, top)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t index = 0; index < 11; ++index) {
            if (damage_named(lua, top, bird_remove_native_strings[index])) {
                output[1] = index + 1u;
                return;
            }
        }
        fail("Unknown original removeBird string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, top, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, top);
        if (output[1] < 201 || output[1] >= 201 + BIRD_REMOVE_TABLE_COUNT) {
            fail("Unknown original removeBird table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original removeBird value\n");
    }
}

static void bird_remove_native_field(void *lua, BirdRemoveTable table,
                                     const char *field, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    bird_remove_native_table(lua, table);
    damage_lua.get_field(lua, top + 1, field);
    bird_remove_native_value(lua, output);
    damage_lua.set_top(lua, top);
}

static void bird_remove_native_snapshot(void *lua,
                                        uint32_t output[BIRD_REMOVE_STATE_WORDS]) {
    bird_remove_native_field(lua, BIRD_REMOVE_BIRD, "name", output);
    bird_remove_native_field(lua, BIRD_REMOVE_HAT_BODY, "defence", output + 2);
    bird_remove_native_field(lua, BIRD_REMOVE_HAT_BODY, "strength", output + 4);
    const char *const globals[] = {
        "currentBirdName", "flyingBird", "birdSpecialtyAvailable", "objects", "birds",
        "otherBirds",      "g_birdHats"};
    int top = damage_lua.get_top(lua);
    size_t next = 6;
    for (size_t index = 0; index < 7; ++index) {
        damage_lua.get_field(lua, -10002, globals[index]);
        bird_remove_native_value(lua, output + next);
        damage_lua.set_top(lua, top);
        next += 2;
    }
    const BirdRemoveTable maps[] = {BIRD_REMOVE_WORLD_A, BIRD_REMOVE_DYNAMIC_A,
                                    BIRD_REMOVE_BIRDS_A, BIRD_REMOVE_OTHERS_A,
                                    BIRD_REMOVE_WORLD_B, BIRD_REMOVE_DYNAMIC_B,
                                    BIRD_REMOVE_BIRDS_B, BIRD_REMOVE_OTHERS_B};
    for (size_t index = 0; index < 8; ++index) {
        bird_remove_native_field(lua, maps[index], "Bird", output + next);
        bird_remove_native_field(lua, maps[index], "Renamed", output + next + 2);
        next += 4;
    }
    for (size_t index = 0; index < 2; ++index) {
        BirdRemoveTable container =
            index ? BIRD_REMOVE_OBJECTS_B : BIRD_REMOVE_OBJECTS_A;
        bird_remove_native_field(lua, container, "world", output + next);
        bird_remove_native_field(lua, container, "dynamic", output + next + 2);
        next += 4;
    }
    if (next != BIRD_REMOVE_STATE_WORDS) {
        fail("Original removeBird snapshot size mismatch\n");
    }
}

static uint32_t bird_remove_native_pc(void *lua) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u) {
        fail("Missing original removeBird caller\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original removeBird function\n");
    }
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || closure[7] || !prototype || prototype[11] != 126) {
        fail("Unexpected original removeBird closure\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (!code || code[0] != UINT32_C(0x00c00017) || code[125] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 504u || (saved - begin) % 4u) {
        fail("Unexpected original removeBird instructions\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static void bird_remove_native_bind_maps(void *lua, int second) {
    const BirdRemoveTable maps[] = {
        second ? BIRD_REMOVE_OBJECTS_B : BIRD_REMOVE_OBJECTS_A,
        second ? BIRD_REMOVE_BIRDS_B : BIRD_REMOVE_BIRDS_A,
        second ? BIRD_REMOVE_OTHERS_B : BIRD_REMOVE_OTHERS_A};
    const char *const names[] = {"objects", "birds", "otherBirds"};
    for (size_t index = 0; index < 3; ++index) {
        uint32_t error = bird_remove_native_input[2];
        if ((index == 1 && error == 13) || (index == 2 && error == 14)) {
            damage_lua.push_boolean(lua, 0);
        } else {
            bird_remove_native_table(lua, maps[index]);
        }
        damage_lua.set_field(lua, -10002, names[index]);
    }
}

static void bird_remove_native_mutate(void *lua, uint32_t kind) {
    uint32_t flags = bird_remove_native_input[1];
    int top = damage_lua.get_top(lua);
    if (flags & 256u) {
        if (kind == 1 || kind == 3 || kind == 4) {
            bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
            damage_lua.push_string(lua, kind == 3 ? "Bird" : "Renamed");
            damage_lua.set_field(lua, top + 1, "name");
            damage_lua.set_top(lua, top);
        } else if (kind == 6 || kind == 11) {
            bird_remove_native_bind_maps(lua, kind == 6);
        } else if (kind == 9) {
            damage_function(lua, -10002, "playAudio", bird_remove_native_audio_two);
        }
    }
    if (kind == 6 && (flags & 128u)) {
        bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
        damage_lua.get_field(lua, top + 1, "name");
        damage_lua.set_field(lua, -10002, "currentBirdName");
        damage_lua.set_top(lua, top);
        bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
        damage_lua.set_field(lua, -10002, "flyingBird");
    }
}

static void bird_remove_native_event(void *lua, uint32_t kind, uint32_t pc,
                                     uint32_t variant, int arguments) {
    if (bird_remove_native_pc(lua) != pc || damage_lua.get_top(lua) != arguments ||
        bird_remove_native_count >= BIRD_REMOVE_MAX_EVENTS || arguments > 4) {
        fail("Unexpected original removeBird boundary\n");
    }
    uint32_t *event = bird_remove_native_events[bird_remove_native_count++];
    for (size_t index = 0; index < BIRD_REMOVE_EVENT_WORDS; ++index) {
        event[index] = 0;
    }
    event[0] = pc;
    event[1] = kind;
    event[2] = variant;
    event[3] = (uint32_t)arguments;
    /* Capture each argument without coercing later strings or table identities. */
    for (int index = 1; index <= arguments; ++index) {
        bird_remove_native_push(lua, index);
        bird_remove_native_value(lua, event + 4 + (size_t)(index - 1) * 2);
        damage_lua.set_top(lua, arguments);
    }
    bird_remove_native_snapshot(lua, event + 12);
    bird_remove_native_mutate(lua, kind);
}

static int bird_remove_native_destroy(void *lua) {
    bird_remove_native_event(lua, 1, 13, 0, 1);
    damage_lua.push_boolean(lua, 0);
    damage_lua.push_number(lua, 0);
    return 2;
}

static int bird_remove_native_definition(void *lua) {
    uint32_t pc = bird_remove_native_pc(lua);
    if (pc != 16 && pc != 91) {
        fail("Unexpected original removeBird definition branch\n");
    }
    bird_remove_native_event(lua, 2, pc, 0, 1);
    if (pc == 91 && bird_remove_native_input[2] == 15) {
        damage_lua.push_boolean(lua, 0);
    } else {
        bird_remove_native_table(lua, BIRD_REMOVE_DEFINITION);
    }
    damage_lua.push_nil(lua);
    return 2;
}

static int bird_remove_native_hook(void *lua) {
    bird_remove_native_event(lua, 3, 27, 0, 1);
    return 0;
}

static int bird_remove_native_property(void *lua) {
    bird_remove_native_event(lua, 4, 58, 0, 2);
    if (bird_remove_native_input[1] & 64u) {
        bird_remove_native_table(lua, BIRD_REMOVE_PARTICLES);
    } else {
        damage_lua.push_boolean(lua, 0);
    }
    return 1;
}

static int bird_remove_native_emit(void *lua) {
    bird_remove_native_event(lua, 5, 69, 0, 4);
    return 0;
}

static int bird_remove_native_camera(void *lua) {
    bird_remove_native_event(lua, 6, 73, 0, 2);
    return 0;
}

static int bird_remove_native_volume(void *lua) {
    bird_remove_native_event(lua, 8, 104, 0, 2);
    FloatBits volume = {.bits = bird_remove_native_input[4]};
    damage_lua.push_number(lua, volume.number);
    return 1;
}

static int bird_remove_native_audio_name(void *lua) {
    bird_remove_native_event(lua, 9, 108, 0, 1);
    damage_lua.push_string(lua, "resolved");
    return 1;
}

static int bird_remove_native_audio_one(void *lua) {
    bird_remove_native_event(lua, 10, 111, 1, 3);
    return 0;
}

static int bird_remove_native_audio_two(void *lua) {
    bird_remove_native_event(lua, 10, 111, 2, 3);
    return 0;
}

static int bird_remove_native_remove(void *lua) {
    bird_remove_native_event(lua, 11, 114, 0, 1);
    return 0;
}

static void bird_remove_native_function(void *lua, int table, const char *name,
                                        int (*function)(void *), uint32_t error) {
    if (bird_remove_native_input[2] == error) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, table, name);
    } else {
        damage_function(lua, table, name, function);
    }
}

static void bird_remove_native_prepare(void *lua) {
    uint32_t flags = bird_remove_native_input[1], error = bird_remove_native_input[2];
    for (uint32_t index = 0; index < BIRD_REMOVE_TABLE_COUNT; ++index) {
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_number(lua, (float)(index + 201u));
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, bird_remove_native_tables[index]);
    }
    bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
    damage_lua.push_string(lua, "Bird");
    damage_lua.set_field(lua, 1, "name");
    damage_lua.push_boolean(lua, (flags & 8u) != 0);
    damage_lua.set_field(lua, 1, "inBubble");
    if (flags & 1u) {
        bird_remove_native_table(lua, BIRD_REMOVE_SCRIPT);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "gameObjectScript");
    damage_lua.set_top(lua, 0);
    bird_remove_native_table(lua, BIRD_REMOVE_SCRIPT);
    bird_remove_native_function(lua, 1, "onDestroy", bird_remove_native_destroy, 1);
    bird_remove_native_function(lua, 1, "get", bird_remove_native_property, 5);
    damage_lua.set_top(lua, 0);
    bird_remove_native_table(lua, BIRD_REMOVE_DEFINITION);
    if (flags & 2u) {
        damage_lua.push_string(lua, "hook");
    } else {
        damage_lua.push_boolean(lua, 0);
    }
    damage_lua.set_field(lua, 1, "onBirdRemoved");
    bird_remove_native_scalar(lua, flags & 64u ? bird_remove_native_input[3] : 1u);
    damage_lua.set_field(lua, 1, "destroyedInsideBubbleSound");
    damage_lua.set_top(lua, 0);
    bird_remove_native_table(lua, BIRD_REMOVE_HATS);
    for (size_t index = 0; index < 2; ++index) {
        if (flags & 4u) {
            bird_remove_native_table(lua, BIRD_REMOVE_HAT_ENTRY);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, 1, index ? "Renamed" : "Bird");
    }
    damage_lua.set_top(lua, 0);
    bird_remove_native_table(lua, BIRD_REMOVE_HAT_ENTRY);
    damage_lua.push_string(lua, "Hat");
    damage_lua.set_field(lua, 1, "hat");
    damage_lua.set_top(lua, 0);
    bird_remove_native_table(lua, BIRD_REMOVE_HAT_BODY);
    damage_number(lua, 1, "defence", UINT32_C(0x41200000));
    damage_number(lua, 1, "strength", UINT32_C(0x41a00000));
    damage_lua.set_top(lua, 0);
    for (uint32_t group = 0; group < 2; ++group) {
        uint32_t start = group ? BIRD_REMOVE_WORLD_B : BIRD_REMOVE_WORLD_A;
        for (uint32_t map = 0; map < 4; ++map) {
            bird_remove_native_table(lua, (BirdRemoveTable)(start + map));
            bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
            damage_lua.set_field(lua, 1, "Bird");
            bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
            damage_lua.set_field(lua, 1, "Renamed");
            if (!map) {
                bird_remove_native_table(lua, BIRD_REMOVE_HAT_BODY);
                damage_lua.set_field(lua, 1, "Hat");
            }
            damage_lua.set_top(lua, 0);
        }
        bird_remove_native_table(lua,
                                 group ? BIRD_REMOVE_OBJECTS_B : BIRD_REMOVE_OBJECTS_A);
        bird_remove_native_table(lua, (BirdRemoveTable)start);
        damage_lua.set_field(lua, 1, "world");
        if (error == 12) {
            damage_lua.push_boolean(lua, 0);
        } else {
            bird_remove_native_table(lua, (BirdRemoveTable)(start + 1u));
        }
        damage_lua.set_field(lua, 1, "dynamic");
        damage_lua.set_top(lua, 0);
    }
    bird_remove_native_table(lua, BIRD_REMOVE_PARTICLES);
    bird_remove_native_scalar(lua, bird_remove_native_input[3]);
    damage_lua.set_field(lua, 1, "removal");
    damage_lua.set_top(lua, 0);
    bird_remove_native_table(lua, BIRD_REMOVE_CAMERA);
    bird_remove_native_function(lua, 1, "onRemoveBird", bird_remove_native_camera, 7);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    bird_remove_native_table(lua, BIRD_REMOVE_EFFECT);
    bird_remove_native_function(lua, 1, "birdRemovalVolume", bird_remove_native_volume,
                                8);
    if (!(flags & 32u)) {
        damage_lua.set_top(lua, 0);
        damage_lua.push_boolean(lua, 0);
    }
    damage_lua.set_field(lua, -10002, "g_levelEffect");
    bird_remove_native_bind_maps(lua, 0);
    if (error == 4) {
        damage_lua.push_boolean(lua, 0);
    } else {
        bird_remove_native_table(lua, BIRD_REMOVE_HATS);
    }
    damage_lua.set_field(lua, -10002, "g_birdHats");
    damage_lua.push_string(lua, flags & 128u ? "Other" : "Bird");
    damage_lua.set_field(lua, -10002, "currentBirdName");
    bird_remove_native_table(lua,
                             flags & 128u ? BIRD_REMOVE_HAT_BODY : BIRD_REMOVE_BIRD);
    damage_lua.set_field(lua, -10002, "flyingBird");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "birdSpecialtyAvailable");
    bird_remove_native_function(lua, -10002, "getObjectDefinition",
                                bird_remove_native_definition, 2);
    bird_remove_native_function(lua, -10002, "hook", bird_remove_native_hook, 3);
    bird_remove_native_function(lua, -10002, "addParticlesNew", bird_remove_native_emit,
                                6);
    bird_remove_native_function(lua, -10002, "getAudioName",
                                bird_remove_native_audio_name, 9);
    bird_remove_native_function(lua, -10002, "playAudio", bird_remove_native_audio_one,
                                10);
    bird_remove_native_function(lua, -10002, "removeObject", bird_remove_native_remove,
                                11);
}

static int bird_remove_native_error(const char *message, const char *wanted) {
    for (size_t start = 0; message && message[start]; ++start) {
        size_t index = 0;
        while (wanted[index] && message[start + index] == wanted[index]) {
            ++index;
        }
        if (!wanted[index]) {
            return 1;
        }
    }
    return 0;
}

static void probe_bird_remove(void) {
    initialize_damage_lua();
    bird_remove_native_push =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removeBird state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    char chunk[1118];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-bird.lua", NULL, 0);
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk) ||
        system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, chunk, sizeof(chunk), "original-remove-bird")) {
        fail("Cannot load original removeBird prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_bremove_function");
    semihost(4, "AB_ANDROID_BIRD_REMOVE_V1 1024\n");
    for (uint32_t sample = 0; sample < 1024; ++sample) {
        bird_remove_case(sample, bird_remove_native_input);
        print_words(bird_remove_native_input, BIRD_REMOVE_INPUT_WORDS);
        bird_remove_native_count = 0;
        bird_remove_native_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_bremove_function");
        bird_remove_native_table(lua, BIRD_REMOVE_BIRD);
        bird_remove_native_scalar(lua, bird_remove_native_input[5]);
        bird_remove_native_scalar(lua, bird_remove_native_input[3]);
        uint32_t failed = damage_lua.call(lua, 3, 0, 0) != 0;
        if (failed) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (!bird_remove_native_error(message, "index") &&
                !bird_remove_native_error(message, "call")) {
                fail(message ? message : "Unknown original removeBird error\n");
            }
        }
        damage_lua.set_top(lua, 0);
        print_words(&failed, 1);
        uint32_t state[BIRD_REMOVE_STATE_WORDS];
        bird_remove_native_snapshot(lua, state);
        print_words(state, BIRD_REMOVE_STATE_WORDS);
        print_words(&bird_remove_native_count, 1);
        for (uint32_t event = 0; event < bird_remove_native_count; ++event) {
            print_words(bird_remove_native_events[event], BIRD_REMOVE_EVENT_WORDS);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird remove reference complete\n");
}

#endif
