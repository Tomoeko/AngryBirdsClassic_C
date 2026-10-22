#include "ab/block_collision.h"
#include "ab/damage_particles.h"
#include "block_collision_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Block collision failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct BlockHost BlockHost;
typedef struct {
    BlockHost *host;
    size_t index;
} ObjectContext;

struct BlockHost {
    AbBlockCollisionHost callbacks;
    AbBlockCollisionObject objects[2];
    AbBlockCollisionDefinition definitions[2];
    AbBlockCollisionScript scripts[2];
    ObjectContext contexts[2];
    AbDataEntry descriptor_entries[2][2], raw_entries[2];
    AbDataTable descriptors[2], raw[2];
    AbBlockCollisionScores scores;
    float score_entries[4];
    uint32_t input[15], events[64][12], count;
    size_t words;
    bool fail_named;
};

static const char *const names[] = {"first", "second"};
static const char *const fields[] = {
    "old1",   "old2",   "new1",   "new2",          "blink1",        "blink2",
    "smile1", "smile2", "truthy", "first_handler", "second_handler"};
static const char *const sounds[] = {"damage1", "damage2", "contact1", "contact2",
                                     "audio"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static uint32_t bits(float value) {
    return block_collision_bits(value);
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint32_t string_id(AbSpan value, const char *const *strings, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (value.size == strlen(strings[index]) && value.data &&
            memcmp(value.data, strings[index], value.size) == 0) {
            return (uint32_t)index + 1;
        }
    }
    return 0;
}

static uint32_t object_id(const BlockHost *host, const AbBlockCollisionObject *object) {
    for (uint32_t index = 0; index < 2; ++index) {
        if (object == host->objects + index) {
            return index + 1;
        }
    }
    return 0;
}

static AbDataValue scalar(uint32_t kind) {
    static AbDataTable empty = {0};
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN};
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER};
        case 4:
            return text("truthy");
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static uint32_t *record(BlockHost *host, uint32_t kind, uint32_t self, uint32_t other) {
    if (host->count >= 64) {
        return NULL;
    }
    uint32_t *event = host->events[host->count++];
    event[0] = kind;
    event[1] = self;
    event[2] = other;
    event[9] = bits(host->scores.total);
    for (size_t object = 0; object < 2; ++object) {
        AbDataValue sprite = host->objects[object].damage_sprite;
        if (sprite.kind != AB_DATA_STRING ||
            !(event[10 + object] = string_id(sprite.string, fields, 11))) {
            return NULL;
        }
    }
    return event;
}

static bool notice_event(BlockHost *host, uint32_t kind,
                         const AbBlockCollisionNotice *notice) {
    uint32_t self = object_id(host, notice->first),
             other = object_id(host, notice->second);
    CHECK(self && other);
    uint32_t *event = record(host, kind, self, other);
    CHECK(event);
    event[3] = bits(notice->force);
    event[4] = bits(notice->damage);
    if (notice->normal_present) {
        event[5] = bits(notice->normal.x);
        event[6] = bits(notice->normal.y);
    }
    if (notice->point_present) {
        event[7] = bits(notice->point.x);
        event[8] = bits(notice->point.y);
    }
    return true;
}

static bool lookup(void *context, AbSpan name, AbBlockCollisionObject **output,
                   AbError *error) {
    (void)error;
    BlockHost *host = context;
    uint32_t id = string_id(name, names, 2);
    CHECK(id);
    *output = host->objects + id - 1u;
    return true;
}

static bool level(void *context, const AbBlockCollisionNotice *notice,
                  AbDataValue *result, AbError *error) {
    (void)error;
    BlockHost *host = context;
    CHECK(!notice->normal_present && !notice->point_present &&
          notice_event(host, BLOCK_EVENT_LEVEL, notice));
    *result = scalar(host->input[4]);
    return true;
}

static bool definition(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error) {
    (void)error;
    BlockHost *host = context;
    uint32_t id = string_id(name, names, 2);
    CHECK(id && record(host, BLOCK_EVENT_DEFINITION, id, 0));
    *output = host->definitions + id - 1u;
    return true;
}

static bool named(void *context, AbDataValue name, const AbBlockCollisionNotice *notice,
                  AbError *error) {
    BlockHost *host = context;
    CHECK(name.kind == AB_DATA_STRING);
    uint32_t id = string_id(name.string, fields, 11);
    CHECK((id == 10 || id == 11) && notice->normal_present && !notice->point_present &&
          notice_event(host,
                       id == 10 ? BLOCK_EVENT_NAMED_FIRST : BLOCK_EVENT_NAMED_SECOND,
                       notice));
    if (host->fail_named) {
        return ab_fail(error, "Controlled named collision failure");
    }
    if (id == 10 && (host->input[14] & 1u)) {
        host->definitions[1].on_collision = (AbDataValue){0};
    }
    return true;
}

static bool script_event(void *context, const AbBlockCollisionNotice *notice,
                         AbError *error) {
    (void)error;
    CHECK(notice->normal_present && !notice->point_present);
    return notice_event(context, BLOCK_EVENT_SCRIPTS, notice);
}

static bool game_script(void *context, const AbBlockCollisionNotice *notice,
                        AbError *error) {
    (void)error;
    ObjectContext *object = context;
    BlockHost *host = object->host;
    CHECK(notice->first == host->objects + object->index && notice->normal_present &&
          notice->point_present &&
          string_id(notice->other_name, names, 2) == object_id(host, notice->second) &&
          notice_event(host, BLOCK_EVENT_GAME_SCRIPT, notice));
    if (!object->index && (host->input[14] & 2u)) {
        host->objects[1].sensor = scalar(1);
    }
    return true;
}

static bool sprites(void *context, AbBlockCollisionObject *object,
                    AbDamageSpriteNames *output, AbError *error) {
    (void)error;
    BlockHost *host = context;
    uint32_t id = object_id(host, object);
    CHECK(id);
    uint32_t *event = record(host, BLOCK_EVENT_SPRITES, id, 0);
    CHECK(event);
    event[3] = id + 2;
    if (id == 1 && (host->input[14] & 4u)) {
        host->objects[1].damage_sprite = text("new2");
    }
    *output = (AbDamageSpriteNames){.sprite = text(id == 1 ? "new1" : "new2"),
                                    .blink = id == 1 ? scalar(1) : text("blink2"),
                                    .smile = id == 1 ? scalar(0) : text("smile2")};
    return true;
}

static bool sound(void *context, AbBlockCollisionSound kind,
                  AbBlockCollisionObject *object, AbBlockCollisionDefinition *resolved,
                  AbDataValue *output, AbError *error) {
    (void)error;
    BlockHost *host = context;
    uint32_t id = object_id(host, object);
    CHECK(id && resolved == host->definitions + id - 1u);
    bool contact = kind == AB_BLOCK_COLLISION_CONTACT_SOUND;
    uint32_t name = (contact ? 2u : 0u) + id;
    bool present = (host->input[13] & (1u << (name - 1u))) != 0;
    uint32_t *event = record(
        host, contact ? BLOCK_EVENT_CONTACT_SOUND : BLOCK_EVENT_DAMAGE_SOUND, id, id);
    CHECK(event);
    event[3] = present ? name : 0;
    event[4] = present ? 4u : host->input[0] & 1u;
    *output = present ? text(sounds[name - 1u]) : scalar(host->input[0] & 1u);
    return true;
}

static bool replaced_audio(void *context, AbAudioPlay request, AbError *error);

static bool audio_name(void *context, AbDataValue value, AbDataValue *output,
                       AbError *error) {
    (void)error;
    BlockHost *host = context;
    CHECK(value.kind == AB_DATA_STRING);
    uint32_t id = string_id(value.string, sounds, 4);
    uint32_t *event = record(host, BLOCK_EVENT_AUDIO_NAME, 0, 0);
    CHECK(id && event);
    event[3] = id;
    event[4] = 5;
    if (host->input[14] & 8u) {
        host->callbacks.sound_force_threshold = 0;
    }
    if (host->input[14] & 32u) {
        host->callbacks.audio = replaced_audio;
    }
    *output = text("audio");
    return true;
}

static bool audio_event(BlockHost *host, AbAudioPlay request, uint32_t kind) {
    CHECK(request.name.kind == AB_DATA_STRING &&
          string_id(request.name.string, sounds, 5) == 5 &&
          request.volume.kind == AB_DATA_NUMBER &&
          request.volume.bits == UINT32_C(0x3f000000) &&
          request.loop.kind == AB_DATA_BOOLEAN && !request.loop.bits &&
          request.channel.kind == AB_DATA_NUMBER &&
          request.channel.bits == UINT32_C(0x40000000));
    uint32_t *event = record(host, kind, 0, 0);
    CHECK(event);
    event[3] = 5;
    event[4] = request.volume.bits;
    event[6] = request.channel.bits;
    return true;
}

static bool audio(void *context, AbAudioPlay request, AbError *error) {
    (void)error;
    return audio_event(context, request, BLOCK_EVENT_AUDIO);
}

static bool replaced_audio(void *context, AbAudioPlay request, AbError *error) {
    (void)error;
    return audio_event(context, request, BLOCK_EVENT_AUDIO_REPLACED);
}

static bool transform(void *context, AbVec2 point, AbVec2 *output, AbError *error) {
    (void)error;
    ObjectContext *object = context;
    BlockHost *host = object->host;
    CHECK(point.x == (float)(object->index + 1) && point.y == -point.x);
    uint32_t *event =
        record(host, BLOCK_EVENT_TRANSFORM, (uint32_t)object->index + 1, 0);
    CHECK(event);
    *output = (AbVec2){point.x * 20, point.y * 20};
    event[3] = bits(point.x);
    event[4] = bits(point.y);
    event[5] = bits(output->x);
    event[6] = bits(output->y);
    return true;
}

static bool emit(void *context, const AbAmbientParticleEmission *emission,
                 AbError *error) {
    (void)error;
    static const char *const particles[] = {"part1", "part2"};
    ObjectContext *object = context;
    BlockHost *host = object->host;
    uint32_t id = string_id(emission->particles, particles, 2);
    CHECK(id == object->index + 1);
    uint32_t *event = record(host, BLOCK_EVENT_EMIT, id, 0);
    CHECK(event);
    event[3] = bits(emission->amount);
    event[4] = bits(emission->position.x);
    event[5] = bits(emission->position.y);
    return true;
}

static bool particles(void *context, AbBlockCollisionObject *object, AbError *error) {
    BlockHost *host = context;
    uint32_t id = object_id(host, object);
    CHECK(id);
    ObjectContext *entry = host->contexts + id - 1u;
    AbVec2 position = {(float)id, -(float)id};
    return ab_damage_state_particles(&host->raw[id - 1u], &position,
                                     (AbAmbientParticleHost){entry, transform, emit},
                                     error);
}

static bool score(void *context, float damage, float *output, AbError *error) {
    (void)error;
    BlockHost *host = context;
    uint32_t *event = record(host, BLOCK_EVENT_SCORE, 0, 0);
    CHECK(event);
    event[3] = bits(damage);
    if (host->input[14] & 16u) {
        host->scores.total = 100;
    }
    *output = number(host->input[12]);
    return true;
}

static void prepare(BlockHost *host, uint32_t sample) {
    *host = (BlockHost){0};
    block_collision_case(sample, host->input);
    host->scores =
        (AbBlockCollisionScores){number(host->input[11]), host->score_entries, 0, 4};
    host->callbacks =
        (AbBlockCollisionHost){.context = host,
                               .object = lookup,
                               .level = host->input[3] & 1u ? level : NULL,
                               .definition = definition,
                               .named = named,
                               .scripts = script_event,
                               .sprites = sprites,
                               .sound = sound,
                               .audio_name = audio_name,
                               .audio = audio,
                               .particles = particles,
                               .score = score,
                               .scores = &host->scores,
                               .sound_force_threshold = 1};
    for (size_t index = 0; index < 2; ++index) {
        host->contexts[index] = (ObjectContext){host, index};
        host->scripts[index] =
            (AbBlockCollisionScript){host->contexts + index, game_script};
        bool changed = (host->input[3] & (32u << index)) != 0;
        host->objects[index] = (AbBlockCollisionObject){
            .name = text(names[index]).string,
            .sensor = scalar(host->input[1 + index]),
            .damage_sprite = text(fields[(changed ? 0u : 2u) + index]),
            .script = host->input[3] & (8u << index) ? host->scripts + index : NULL,
            .context = host->contexts + index};
        host->definitions[index] = (AbBlockCollisionDefinition){
            .on_collision =
                host->input[3] & (2u << index) ? text(fields[9 + index]) : scalar(0),
            .context = host->contexts + index};
        host->descriptor_entries[index][0] =
            (AbDataEntry){text("name"), text(index ? "part2" : "part1")};
        host->descriptor_entries[index][1] = (AbDataEntry){
            text("amount"), {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)}};
        host->descriptors[index] =
            (AbDataTable){host->descriptor_entries[index], 2, 2, NULL};
        host->raw_entries[index] =
            (AbDataEntry){text("damageStateChangedParticles"),
                          host->input[3] & (128u << index)
                              ? (AbDataValue){.kind = AB_DATA_TABLE,
                                              .table = host->descriptors + index}
                              : scalar(0)};
        host->raw[index] = (AbDataTable){host->raw_entries + index, 1, 1, NULL};
    }
}

static AbBlockCollisionInput input(const BlockHost *host) {
    return (AbBlockCollisionInput){text("first").string,
                                   text("second").string,
                                   number(host->input[5]),
                                   number(host->input[6]),
                                   {number(host->input[7]), number(host->input[8])},
                                   {number(host->input[9]), number(host->input[10])}};
}

static bool value_words(AbDataValue value, uint32_t output[2]) {
    output[1] = 0;
    switch (value.kind) {
        case AB_DATA_NIL:
            output[0] = 0;
            break;
        case AB_DATA_BOOLEAN:
            output[0] = 1;
            output[1] = value.bits != 0;
            break;
        case AB_DATA_NUMBER:
            output[0] = 3;
            output[1] = value.bits;
            break;
        case AB_DATA_STRING:
            output[0] = 4;
            output[1] = string_id(value.string, fields, 11);
            CHECK(output[1]);
            break;
        case AB_DATA_TABLE:
            output[0] = 5;
            break;
        case AB_DATA_OBJECT:
            return false; /* This native fixture has no C-instance identity map. */
    }
    return true;
}

static bool compare(FILE *file, BlockHost *host, const char *section,
                    const uint32_t *actual, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[field]) {
            fprintf(stderr,
                    "Block case %" PRIu32 " %s[%zu]: C %08" PRIx32 ", native %08" PRIx32
                    "\n",
                    host->input[0], section, field, actual[field], expected);
            return false;
        }
    }
    CHECK(fgetc(file) == '\n');
    host->words += count;
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_BLOCK_COLLISION_V1 4096\n") == 0);
    size_t words = 0, calls[16] = {0}, scores = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        BlockHost host;
        prepare(&host, sample);
        AbBlockCollisionInput request = input(&host);
        CHECK(compare(file, &host, "input", host.input, 15) &&
              ab_block_collision_callback(&request, &host.callbacks, NULL) &&
              host.scores.count <= 2);
        uint32_t summary[] = {bits(host.scores.total), (uint32_t)host.scores.count,
                              host.scores.count ? bits(host.score_entries[0]) : 0,
                              host.scores.count > 1 ? bits(host.score_entries[1]) : 0,
                              bits(host.callbacks.sound_force_threshold)};
        CHECK(compare(file, &host, "score/threshold", summary, 5));
        scores += host.scores.count;
        for (size_t object = 0; object < 2; ++object) {
            const AbBlockCollisionObject *entry = host.objects + object;
            AbDataValue values[] = {entry->sensor, entry->damage_sprite,
                                    entry->blink_sprite, entry->smile_sprite};
            uint32_t output[11];
            for (size_t field = 0; field < 4; ++field) {
                CHECK(value_words(values[field], output + field * 2));
            }
            output[8] = entry->script != NULL;
            CHECK(value_words(host.definitions[object].on_collision, output + 9) &&
                  compare(file, &host, "object/definition", output, 11));
        }
        CHECK(compare(file, &host, "event count", &host.count, 1));
        for (size_t event = 0; event < host.count; ++event) {
            if (!compare(file, &host, "event", host.events[event], 12)) {
                fprintf(stderr, "Block event %zu kind %" PRIu32 "\n", event,
                        host.events[event][0]);
                return false;
            }
            CHECK(host.events[event][0] < 16);
            ++calls[host.events[event][0]];
        }
        words += host.words;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB block collision reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4,096 original block collision callbacks match: %zu words, %zu "
           "script events, %zu object methods, %zu particle emissions and %zu score "
           "appends.\n",
           words, calls[BLOCK_EVENT_SCRIPTS], calls[BLOCK_EVENT_GAME_SCRIPT],
           calls[BLOCK_EVENT_EMIT], scores);
    for (size_t kind = 1; kind < 16; ++kind) {
        printf("Boundary %zu: %zu calls\n", kind, calls[kind]);
    }
    return true;
}

static bool edge_checks(void) {
    BlockHost host;
    prepare(&host, 14);
    AbBlockCollisionInput request = input(&host);
    request.force = request.damage = NAN;
    AbBlockCollisionHost minimal = {.context = &host, .object = lookup};
    CHECK(ab_block_collision_callback(&request, &minimal, NULL) && !host.count);
    prepare(&host, 0);
    request = input(&host);
    host.callbacks.audio = NULL;
    CHECK(!ab_block_collision_callback(&request, &host.callbacks, NULL) && host.count &&
          host.events[host.count - 1][0] == BLOCK_EVENT_AUDIO_NAME &&
          string_id(host.objects[0].damage_sprite.string, fields, 11) == 3 &&
          string_id(host.objects[1].damage_sprite.string, fields, 11) == 2);
    prepare(&host, 0);
    request = input(&host);
    host.input[12] = bits(1);
    host.input[4] = 1;
    float before = host.scores.total;
    host.scores.capacity = 0;
    CHECK(!ab_block_collision_callback(&request, &host.callbacks, NULL) &&
          host.scores.total == before + 1 && !host.scores.count &&
          host.events[host.count - 1][0] == BLOCK_EVENT_SCORE);
    prepare(&host, 0);
    request = input(&host);
    host.callbacks.scripts = NULL;
    CHECK(!ab_block_collision_callback(&request, &host.callbacks, NULL) &&
          host.events[host.count - 1][0] == BLOCK_EVENT_NAMED_FIRST);
    prepare(&host, 0);
    request = input(&host);
    host.fail_named = true;
    AbError error = {0};
    CHECK(!ab_block_collision_callback(&request, &host.callbacks, &error) &&
          strcmp(error.message, "Controlled named collision failure") == 0 &&
          host.events[host.count - 1][0] == BLOCK_EVENT_NAMED_FIRST);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    bool okay = edge_checks();
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2) {
        okay = okay && file && native_checks(file);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
