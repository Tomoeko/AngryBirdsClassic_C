#ifndef AB_ORACLE_BLOCK_COLLISION_H
#define AB_ORACLE_BLOCK_COLLISION_H

#include "../../tests/block_collision_cases.h"

#include <stdbool.h>

static uint32_t block_collision_input[15], block_collision_events[64][12];
static uint32_t block_collision_count, block_collision_score_count;
static void (*block_collision_set_index)(void *, int, int);
static void (*block_collision_get_index)(void *, int, int);
static const char *const block_collision_names[] = {"first", "second"};
static const char *const block_collision_fields[] = {
    "old1",   "old2",   "new1",   "new2",          "blink1",        "blink2",
    "smile1", "smile2", "truthy", "first_handler", "second_handler"};

static uint32_t block_collision_string_id(void *lua, int index,
                                          const char *const *names, size_t count) {
    for (size_t name = 0; name < count; ++name) {
        if (damage_named(lua, index, names[name])) {
            return (uint32_t)name + 1;
        }
    }
    fail("Unexpected original block-collision string\n");
    return 0;
}

static uint32_t block_collision_object_id(void *lua, int index) {
    int top = damage_lua.get_top(lua);
    if (damage_lua.type(lua, index) == 5) {
        damage_lua.get_field(lua, index, "_id");
        uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, top);
        if (id == 1 || id == 2) {
            return id;
        }
        fail("Invalid block-collision object identity\n");
    }
    return block_collision_string_id(lua, index, block_collision_names, 2);
}

static void block_collision_scalar(void *lua, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, 0);
            break;
        case 2:
            damage_lua.push_boolean(lua, 1);
            break;
        case 3:
            damage_lua.push_number(lua, 0);
            break;
        case 4:
            damage_lua.push_string(lua, "truthy");
            break;
        default:
            damage_lua.create_table(lua, 0, 0);
            break;
    }
}

static void block_collision_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        output[1] = block_collision_string_id(lua, -1, block_collision_fields, 11);
    }
}

static uint32_t *block_collision_record(void *lua, uint32_t kind, uint32_t self,
                                        uint32_t other) {
    if (block_collision_count >= 64) {
        fail("Original block-collision event capacity exceeded\n");
    }
    uint32_t *event = block_collision_events[block_collision_count++];
    for (size_t field = 0; field < 12; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = self;
    event[2] = other;
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_block_scores");
    damage_lua.get_field(lua, -1, "score");
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original block-collision score snapshot is not numeric\n");
    }
    FloatBits total = {.number = damage_lua.to_number(lua, -1)};
    event[9] = total.bits;
    damage_lua.set_top(lua, top);
    for (size_t object = 0; object < 2; ++object) {
        damage_lua.get_field(lua, -10002, block_collision_names[object]);
        damage_lua.get_field(lua, -1, "damageSprite");
        event[10 + object] =
            block_collision_string_id(lua, -1, block_collision_fields, 11);
        damage_lua.set_top(lua, top);
    }
    return event;
}

static void block_collision_numbers(void *lua, uint32_t *event, int first, int last) {
    for (int argument = first; argument <= last; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Original block-collision numeric argument mismatch\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[3 + argument - first] = value.bits;
    }
}

static int block_collision_level(void *lua) {
    if (damage_lua.get_top(lua) != 4) {
        fail("Original block level hook arity mismatch\n");
    }
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_LEVEL,
                                             block_collision_object_id(lua, 1),
                                             block_collision_object_id(lua, 2));
    block_collision_numbers(lua, event, 3, 4);
    block_collision_scalar(lua, block_collision_input[4]);
    return 1;
}

static int block_collision_definition(void *lua) {
    uint32_t id = block_collision_object_id(lua, 1);
    (void)block_collision_record(lua, BLOCK_EVENT_DEFINITION, id, 0);
    damage_lua.get_field(lua, -10002,
                         id == 1 ? "_ab_block_def_first" : "_ab_block_def_second");
    return 1;
}

static int block_collision_named(void *lua, uint32_t kind) {
    if (damage_lua.get_top(lua) != 6) {
        fail("Original named block hook arity mismatch\n");
    }
    uint32_t *event =
        block_collision_record(lua, kind, block_collision_object_id(lua, 1),
                               block_collision_object_id(lua, 2));
    block_collision_numbers(lua, event, 3, 6);
    if (kind == BLOCK_EVENT_NAMED_FIRST && (block_collision_input[14] & 1u)) {
        damage_lua.get_field(lua, -10002, "_ab_block_def_second");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -2, "onCollision");
    }
    return 0;
}

static int block_collision_named_first(void *lua) {
    return block_collision_named(lua, BLOCK_EVENT_NAMED_FIRST);
}

static int block_collision_named_second(void *lua) {
    return block_collision_named(lua, BLOCK_EVENT_NAMED_SECOND);
}

static int block_collision_scripts(void *lua) {
    if (damage_lua.get_top(lua) != 7 || !damage_named(lua, 2, "collision")) {
        fail("Original block-event arguments mismatch\n");
    }
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_SCRIPTS,
                                             block_collision_object_id(lua, 1),
                                             block_collision_object_id(lua, 3));
    block_collision_numbers(lua, event, 4, 7);
    return 0;
}

static int block_collision_game_script(void *lua) {
    if (damage_lua.get_top(lua) != 8) {
        fail("Original block game-object method arity mismatch\n");
    }
    uint32_t self = block_collision_object_id(lua, 1);
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_GAME_SCRIPT, self,
                                             block_collision_object_id(lua, 2));
    block_collision_numbers(lua, event, 3, 8);
    if (self == 1 && (block_collision_input[14] & 2u)) {
        damage_lua.get_field(lua, -10002, "second");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -2, "sensor");
    }
    return 0;
}

static int block_collision_sprites(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5) {
        fail("Original block damage-sprite query arity mismatch\n");
    }
    uint32_t id = block_collision_object_id(lua, 1);
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_SPRITES, id, 0);
    event[3] = id + 2;
    if (id == 1 && (block_collision_input[14] & 4u)) {
        damage_lua.get_field(lua, -10002, "second");
        damage_lua.push_string(lua, "new2");
        damage_lua.set_field(lua, -2, "damageSprite");
        damage_lua.set_top(lua, 2);
    }
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_string(lua, id == 1 ? "new1" : "new2");
    damage_lua.set_field(lua, 3, "sprite");
    if (id == 1) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_string(lua, "blink2");
    }
    damage_lua.set_field(lua, 3, "blink");
    if (id == 2) {
        damage_lua.push_string(lua, "smile2");
        damage_lua.set_field(lua, 3, "smile");
    }
    return 1;
}

static const char *const block_collision_sounds[] = {"damage1", "damage2", "contact1",
                                                     "contact2", "audio"};

static int block_collision_sound(void *lua, bool contact) {
    if (damage_lua.get_top(lua) != 3) {
        fail("Original block level-effect method arity mismatch\n");
    }
    uint32_t id = block_collision_object_id(lua, 2);
    uint32_t sound = (contact ? 2u : 0u) + id;
    bool present = (block_collision_input[13] & (1u << (sound - 1u))) != 0;
    uint32_t *event = block_collision_record(
        lua, contact ? BLOCK_EVENT_CONTACT_SOUND : BLOCK_EVENT_DAMAGE_SOUND, id,
        block_collision_object_id(lua, 3));
    event[3] = present ? sound : 0;
    event[4] = present ? 4u : block_collision_input[0] & 1u;
    if (present) {
        damage_lua.push_string(lua, block_collision_sounds[sound - 1u]);
    } else if (block_collision_input[0] & 1u) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_nil(lua);
    }
    return 1;
}

static int block_collision_damage_sound(void *lua) {
    return block_collision_sound(lua, false);
}

static int block_collision_contact_sound(void *lua) {
    return block_collision_sound(lua, true);
}

static int block_collision_replaced_audio(void *lua);

static int block_collision_audio_name(void *lua) {
    uint32_t sound = block_collision_string_id(lua, 1, block_collision_sounds, 4);
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_AUDIO_NAME, 0, 0);
    event[3] = sound;
    event[4] = 5;
    if (block_collision_input[14] & 8u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.push_number(lua, 0);
        damage_lua.set_field(lua, -2, "collisionSoundForceThreshold");
    }
    if (block_collision_input[14] & 32u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_function(lua, damage_lua.get_top(lua), "playAudio",
                        block_collision_replaced_audio);
    }
    damage_lua.push_string(lua, "audio");
    return 1;
}

static int block_collision_audio_call(void *lua, uint32_t kind) {
    if (damage_lua.get_top(lua) != 4 || !damage_named(lua, 1, "audio") ||
        damage_lua.type(lua, 3) != 1 || damage_lua.to_boolean(lua, 3)) {
        fail("Original block audio arguments mismatch\n");
    }
    FloatBits volume = {.number = damage_lua.to_number(lua, 2)};
    FloatBits channel = {.number = damage_lua.to_number(lua, 4)};
    if (volume.bits != UINT32_C(0x3f000000) || channel.bits != UINT32_C(0x40000000)) {
        fail("Original block audio volume/channel mismatch\n");
    }
    uint32_t *event = block_collision_record(lua, kind, 0, 0);
    event[3] = 5;
    event[4] = volume.bits;
    event[6] = channel.bits;
    return 0;
}

static int block_collision_audio(void *lua) {
    return block_collision_audio_call(lua, BLOCK_EVENT_AUDIO);
}

static int block_collision_replaced_audio(void *lua) {
    return block_collision_audio_call(lua, BLOCK_EVENT_AUDIO_REPLACED);
}

static int block_collision_transform(void *lua) {
    float x = damage_lua.to_number(lua, 1), y = damage_lua.to_number(lua, 2);
    if ((x != 1 && x != 2) || y != -x || damage_lua.get_top(lua) != 2) {
        fail("Original block particle position mismatch\n");
    }
    uint32_t *event =
        block_collision_record(lua, BLOCK_EVENT_TRANSFORM, (uint32_t)x, 0);
    event[3] = block_collision_bits(x);
    event[4] = block_collision_bits(y);
    event[5] = block_collision_bits(x * 20);
    event[6] = block_collision_bits(y * 20);
    damage_lua.push_number(lua, x * 20);
    damage_lua.push_number(lua, y * 20);
    return 2;
}

static int block_collision_emit(void *lua) {
    static const char *const particles[] = {"part1", "part2"};
    uint32_t id = block_collision_string_id(lua, 1, particles, 2);
    if (damage_lua.get_top(lua) != 9) {
        fail("Original block particle emission arity mismatch\n");
    }
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_EMIT, id, 0);
    block_collision_numbers(lua, event, 2, 4);
    for (int argument = 5; argument <= 7; ++argument) {
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        if (damage_lua.type(lua, argument) != 3 || value.bits) {
            fail("Original block particle trailing numeric mismatch\n");
        }
    }
    for (int argument = 8; argument <= 9; ++argument) {
        if (damage_lua.type(lua, argument) != 1 ||
            damage_lua.to_boolean(lua, argument)) {
            fail("Original block particle trailing Boolean mismatch\n");
        }
    }
    return 0;
}

static int block_collision_score(void *lua) {
    uint32_t *event = block_collision_record(lua, BLOCK_EVENT_SCORE, 0, 0);
    block_collision_numbers(lua, event, 1, 1);
    if (block_collision_input[14] & 16u) {
        damage_lua.get_field(lua, -10002, "_ab_block_scores");
        damage_lua.push_number(lua, 100);
        damage_lua.set_field(lua, -2, "score");
    }
    FloatBits increment = {.bits = block_collision_input[12]};
    damage_lua.push_number(lua, increment.number);
    return 1;
}

static int block_collision_insert(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3 || block_collision_score_count >= 2) {
        fail("Original block score append mismatch\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, 2)};
    damage_lua.push_number(lua, value.number);
    block_collision_set_index(lua, 1, (int)++block_collision_score_count);
    return 0;
}

static void block_collision_prepare(void *lua) {
    uint32_t flags = block_collision_input[3];
    for (uint32_t id = 1; id <= 2; ++id) {
        damage_lua.create_table(lua, 0, 9);
        damage_lua.push_number(lua, (float)id);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.push_string(lua, block_collision_names[id - 1u]);
        damage_lua.set_field(lua, 1, "name");
        damage_lua.push_string(lua, id == 1 ? "Def1" : "Def2");
        damage_lua.set_field(lua, 1, "definition");
        damage_lua.push_number(lua, (float)id);
        damage_lua.set_field(lua, 1, "x");
        damage_lua.push_number(lua, -(float)id);
        damage_lua.set_field(lua, 1, "y");
        block_collision_scalar(lua, block_collision_input[id]);
        damage_lua.set_field(lua, 1, "sensor");
        bool changed = (flags & (32u << (id - 1u))) != 0;
        damage_lua.push_string(lua,
                               block_collision_fields[(changed ? 0u : 2u) + id - 1u]);
        damage_lua.set_field(lua, 1, "damageSprite");
        if (flags & (8u << (id - 1u))) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_number(lua, (float)id);
            damage_lua.set_field(lua, 2, "_id");
            damage_function(lua, 2, "onBlockCollision", block_collision_game_script);
            damage_lua.set_field(lua, 1, "gameObjectScript");
        }
        damage_lua.set_field(lua, -10002, block_collision_names[id - 1u]);
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, (float)id);
        damage_lua.set_field(lua, 1, "_id");
        if (flags & (2u << (id - 1u))) {
            damage_lua.push_string(lua, id == 1 ? "first_handler" : "second_handler");
            damage_lua.set_field(lua, 1, "onCollision");
        }
        damage_lua.set_field(lua, -10002,
                             id == 1 ? "_ab_block_def_first" : "_ab_block_def_second");
    }
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 1, "score", block_collision_input[11]);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "collisionScores");
    damage_lua.set_field(lua, -10002, "_ab_block_scores");
    damage_lua.create_table(lua, 0, 2);
    for (uint32_t id = 1; id <= 2; ++id) {
        damage_lua.create_table(lua, 0, 1);
        if (flags & (128u << (id - 1u))) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_string(lua, id == 1 ? "part1" : "part2");
            damage_lua.set_field(lua, 3, "name");
            damage_lua.push_number(lua, 2);
            damage_lua.set_field(lua, 3, "amount");
            damage_lua.set_field(lua, 2, "damageStateChangedParticles");
        }
        damage_lua.set_field(lua, 1, id == 1 ? "Def1" : "Def2");
    }
    damage_lua.set_field(lua, -10002, "_ab_block_raw");
    damage_lua.create_table(lua, 0, 12);
    damage_function(lua, 1, "getObjectDefinition", block_collision_definition);
    damage_function(lua, 1, "getDamageSprite", block_collision_sprites);
    damage_function(lua, 1, "first_handler", block_collision_named_first);
    damage_function(lua, 1, "second_handler", block_collision_named_second);
    damage_function(lua, 1, "getAudioName", block_collision_audio_name);
    damage_function(lua, 1, "playAudio", block_collision_audio);
    damage_function(lua, 1, "g_get_score_to_add_for_damaging", block_collision_score);
    damage_function(lua, 1, "physicsToWorldTransform", block_collision_transform);
    damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, 1, "collisionSoundForceThreshold");
    damage_lua.push_string(lua, "level");
    damage_lua.set_field(lua, 1, "levelName");
    damage_lua.create_table(lua, 0, 1);
    if (flags & 1u) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 3, "onBlockCollision", block_collision_level);
        damage_lua.set_field(lua, 2, "level");
    }
    damage_lua.set_field(lua, 1, "g_level_scripts");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    for (uint32_t object = 0; object < 2; ++object) {
        damage_lua.get_field(lua, -10002, block_collision_names[object]);
        damage_lua.set_field(lua, 3, block_collision_names[object]);
    }
    damage_lua.set_field(lua, 2, "world");
    damage_lua.set_field(lua, 1, "objects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_block_raw");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "blockTable");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 3, "triggerEvent", block_collision_scripts);
    damage_lua.set_field(lua, 2, "BlockScriptManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "collision");
    damage_lua.set_field(lua, 3, "EID_COLLISION");
    damage_lua.set_field(lua, 2, "events");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 2, "damageSound", block_collision_damage_sound);
    damage_function(lua, 2, "collisionSound", block_collision_contact_sound);
    damage_lua.set_field(lua, 1, "g_levelEffect");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_block_scores");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "scoreTable");
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void block_collision_snapshot(void *lua) {
    uint32_t summary[5] = {0, block_collision_score_count, 0, 0, 0};
    damage_lua.get_field(lua, -10002, "_ab_block_scores");
    damage_lua.get_field(lua, 1, "score");
    FloatBits total = {.number = damage_lua.to_number(lua, -1)};
    summary[0] = total.bits;
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "collisionScores");
    for (uint32_t index = 0; index < block_collision_score_count; ++index) {
        block_collision_get_index(lua, 2, (int)index + 1);
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        summary[2 + index] = value.bits;
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 1, "collisionSoundForceThreshold");
    FloatBits threshold = {.number = damage_lua.to_number(lua, -1)};
    summary[4] = threshold.bits;
    damage_lua.set_top(lua, 0);
    print_words(summary, 5);
    for (uint32_t object = 0; object < 2; ++object) {
        uint32_t fields[11];
        damage_lua.get_field(lua, -10002, block_collision_names[object]);
        static const char *const field_names[] = {"sensor", "damageSprite",
                                                  "blinkSprite", "smileSprite"};
        for (uint32_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, 1, field_names[field]);
            block_collision_value(lua, fields + field * 2);
            damage_lua.set_top(lua, 1);
        }
        damage_lua.get_field(lua, 1, "gameObjectScript");
        fields[8] = damage_lua.type(lua, -1) == 5;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(
            lua, -10002, object == 0 ? "_ab_block_def_first" : "_ab_block_def_second");
        damage_lua.get_field(lua, 1, "onCollision");
        block_collision_value(lua, fields + 9);
        damage_lua.set_top(lua, 0);
        print_words(fields, 11);
    }
    print_words(&block_collision_count, 1);
    for (uint32_t event = 0; event < block_collision_count; ++event) {
        print_words(block_collision_events[event], 12);
    }
}

static void probe_block_collision(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original block-collision reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    block_collision_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    block_collision_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "insert", block_collision_insert);
    damage_lua.set_field(lua, -10002, "table");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "addParticles", block_collision_emit);
    damage_lua.set_field(lua, -10002, "particles");
    semihost(4, "AB_ANDROID_BLOCK_COLLISION_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_collision_case(sample, block_collision_input);
        print_words(block_collision_input, 15);
        block_collision_count = block_collision_score_count = 0;
        block_collision_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "blockCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        for (size_t field = 5; field <= 10; ++field) {
            FloatBits value = {.bits = block_collision_input[field]};
            damage_lua.push_number(lua, value.number);
        }
        if (damage_lua.call(lua, 8, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original block-collision callback failed\n");
        }
        damage_lua.set_top(lua, 0);
        block_collision_snapshot(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block collision reference complete\n");
}

#endif
