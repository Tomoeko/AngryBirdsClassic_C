#include "ab/block_removal_effects.h"
#include "block_removal_effects_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal effects failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable tables[4];
    AbDataEntry objects[2][4], definition;
    AbDataValue values[4], particles, amount, sound;
    AbStringMap *maps[2];
    AbBlockRemovalState state;
    AbBlockRemovalEffectsHost host;
    AbBlockRemovalEntry entry;
    uint32_t input[22], events[9][57], count, calls[7];
    bool fail_update, mutate_entry;
} Fixture;

static const char *const strings[] = {"body",      "renamed", "BOMB",         "other",
                                      "smokeBuff", "batBuff", "flameBuff",    "sound",
                                      "changed",   "audio",   "tnt_explodes", "truthy"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind, const char *string) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    return kind == 3   ? (AbDataValue){.kind = AB_DATA_NUMBER}
           : kind == 4 ? text(string)
                       : fixture->values[3];
}

static AbDataValue particle(Fixture *fixture, uint32_t kind) {
    if (kind < 4) {
        return scalar(fixture, kind, "");
    }
    return kind == 7 ? fixture->values[3] : text(strings[kind == 8 ? 3u : kind]);
}

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 12; ++index) {
            if (value.string.size == strlen(strings[index]) &&
                !memcmp(value.string.data, strings[index], value.string.size)) {
                output[1] = index + 1u;
                break;
            }
        }
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    }
}

static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error);
static bool destroyed_sound(void *context, AbDataValue object, AbDataValue definition,
                            AbDataValue *output, AbError *error);
static bool random_value(void *context, float lower, float upper, AbDataValue *output,
                         AbError *error);
static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
static bool explosion_first(void *context, AbDataValue object, AbDataValue definition,
                            AbDataValue sound, AbError *error);
static bool explosion_second(void *context, AbDataValue object, AbDataValue definition,
                             AbDataValue sound, AbError *error);
static bool audio_first(void *context, AbAudioPlay request, AbError *error);
static bool audio_second(void *context, AbAudioPlay request, AbError *error);
static bool update(void *context, AbDataValue object, AbError *error);

static void snapshot(Fixture *fixture, uint32_t output[37]) {
    for (size_t object = 0; object < 2; ++object) {
        for (size_t field = 0; field < 4; ++field) {
            value_words(fixture, fixture->objects[object][field].value,
                        output + object * 8u + field * 2u);
        }
    }
    value_words(fixture, fixture->definition.value, output + 16);
    value_words(fixture, fixture->particles, output + 18);
    value_words(fixture, fixture->amount, output + 20);
    value_words(fixture, fixture->sound, output + 22);
    output[24] = !fixture->state.dead_blocks                      ? 0u
                 : fixture->state.dead_blocks == fixture->maps[0] ? 1u
                                                                  : 2u;
    uintptr_t handle = 0;
    bool present = false;
    (void)ab_string_map_get(fixture->maps[0], span("queued"), &handle, &present, NULL);
    value_words(fixture, present && handle ? *(AbDataValue *)handle : (AbDataValue){0},
                output + 25);
    output[27] = !fixture->host.audio                 ? 0u
                 : fixture->host.audio == audio_first ? 1u
                                                      : 2u;
    output[28] = !fixture->host.explosion                     ? 0u
                 : fixture->host.explosion == explosion_first ? 1u
                                                              : 2u;
    output[29] =
        (fixture->host.property ? 1u : 0u) | (fixture->host.destroyed_sound ? 2u : 0u) |
        (fixture->host.random ? 4u : 0u) | (fixture->host.explosion ? 8u : 0u) |
        (fixture->host.audio_name ? 16u : 0u) |
        (fixture->host.update_destroyed ? 32u : 0u);
    memcpy(output + 30, fixture->calls, sizeof(fixture->calls));
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t variant,
                        uint32_t argc) {
    if (fixture->count >= 9) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    memset(event, 0, 57 * sizeof(*event));
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    snapshot(fixture, event + 20);
    ++fixture->calls[kind - 1u];
    return event;
}

static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error) {
    Fixture *fixture = context;
    (void)error;
    bool particles = key.size == 9 && !memcmp(key.data, "particles", 9);
    CHECK(particles || (key.size == 14 && !memcmp(key.data, "particleAmount", 14)));
    uint32_t *event = record(fixture, 1, particles ? 164u : 173u, 0, 2);
    CHECK(event && object.table == fixture->tables);
    event[4] = 501;
    event[5] = particles ? 1u : 2u;
    if (particles) {
        uint32_t mutation = fixture->input[15];
        if (mutation & 1u) {
            fixture->state.dead_blocks = fixture->maps[1];
        }
        if (mutation & 2u) {
            fixture->definition.value = text("BOMB");
        }
        if (mutation & 4u) {
            fixture->objects[0][3].value = scalar(fixture, 1, "");
        }
        if (mutation & 8u) {
            fixture->host.destroyed_sound = NULL;
        }
        if (mutation & 16u) {
            fixture->host.property = NULL;
        }
        if (mutation & 32u) {
            fixture->objects[0][0].value = text("renamed");
        }
        if (fixture->mutate_entry) {
            fixture->entry.key = span("renamed");
            fixture->entry.object = (AbDataValue){0};
        }
    }
    *output = particles ? fixture->particles : fixture->amount;
    return true;
}

static bool destroyed_sound(void *context, AbDataValue object, AbDataValue definition,
                            AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 2, 169, 0, 3);
    CHECK(event && object.table == fixture->tables &&
          definition.table == fixture->tables + 2);
    event[4] = 601;
    event[5] = 201;
    event[6] = 203;
    uint32_t mutation = fixture->input[16];
    if (mutation & 1u) {
        fixture->amount = scalar(fixture, 1, "");
    }
    if (mutation & 2u) {
        fixture->particles = text("changed");
    }
    if (mutation & 4u) {
        fixture->definition.value = text("other");
    }
    if (mutation & 8u) {
        fixture->state.dead_blocks = NULL;
    }
    if (mutation & 16u) {
        fixture->host.random = NULL;
    }
    if (mutation & 32u) {
        fixture->host.audio = audio_second;
    }
    *output = fixture->sound;
    return true;
}

static bool random_value(void *context, float lower, float upper, AbDataValue *output,
                         AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 3, 182, 0, 2);
    CHECK(event && lower == 12 && upper == 20);
    event[4] = bits(lower);
    event[5] = bits(upper);
    uint32_t mutation = fixture->input[17];
    if (mutation & 1u) {
        fixture->state.dead_blocks = fixture->maps[1];
    }
    if (mutation & 2u) {
        fixture->objects[1][1].value = text("BOMB");
    }
    if (mutation & 4u) {
        fixture->objects[0][1].value = text("BOMB");
    }
    if (mutation & 8u) {
        fixture->objects[0][3].value = scalar(fixture, 2, "");
    }
    if (mutation & 16u) {
        CHECK(ab_string_map_set(fixture->maps[0], span("queued"), 0, false, error));
    }
    if (mutation & 32u) {
        fixture->host.explosion = NULL;
    }
    if (mutation & 64u) {
        fixture->sound = text("changed");
    }
    *output = scalar(fixture, fixture->input[8], "truthy");
    return true;
}

static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error) {
    Fixture *fixture = context;
    (void)error;
    bool explosion = sound.kind == AB_DATA_STRING && sound.string.size == 12 &&
                     !memcmp(sound.string.data, "tnt_explodes", 12);
    uint32_t *event = record(fixture, 4, explosion ? 213u : 223u, 0, 1);
    CHECK(event);
    value_words(fixture, sound, event + 4);
    uint32_t mutation = fixture->input[18];
    if (mutation & 1u) {
        fixture->host.audio = audio_second;
    }
    if (mutation & 2u) {
        fixture->host.explosion = explosion_second;
    }
    if (mutation & 4u) {
        fixture->state.dead_blocks = fixture->maps[1];
    }
    if (mutation & 8u) {
        fixture->state.dead_blocks = NULL;
    }
    if (mutation & 16u) {
        fixture->objects[0][3].value = scalar(fixture, 2, "");
    }
    *output = scalar(fixture, fixture->input[20], "audio");
    return true;
}

static bool explosion(void *context, AbDataValue object, AbDataValue definition,
                      AbDataValue sound, uint32_t variant, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 5, 214, variant, 3);
    CHECK(event && definition.table == fixture->tables + 2);
    value_words(fixture, object, event + 4);
    value_words(fixture, definition, event + 6);
    value_words(fixture, sound, event + 8);
    uint32_t mutation = fixture->input[19];
    if (mutation & 1u) {
        fixture->objects[0][3].value = scalar(fixture, 1, "");
    }
    if (mutation & 2u) {
        fixture->sound = text("changed");
    }
    if (mutation & 4u) {
        fixture->host.audio = NULL;
    }
    if (mutation & 8u) {
        fixture->host.update_destroyed = NULL;
    }
    return true;
}

static bool explosion_first(void *context, AbDataValue object, AbDataValue definition,
                            AbDataValue sound, AbError *error) {
    return explosion(context, object, definition, sound, 1, error);
}

static bool explosion_second(void *context, AbDataValue object, AbDataValue definition,
                             AbDataValue sound, AbError *error) {
    return explosion(context, object, definition, sound, 2, error);
}

static bool audio(void *context, AbAudioPlay request, uint32_t variant,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 6, 227, variant, 4);
    CHECK(event);
    const AbDataValue values[] = {request.name, request.volume, request.loop,
                                  request.channel};
    for (size_t field = 0; field < 4; ++field) {
        value_words(fixture, values[field], event + 4 + field * 2u);
    }
    return true;
}

static bool audio_first(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 1, error);
}

static bool audio_second(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 2, error);
}

static bool update(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 7, 230, 0, 1);
    CHECK(event && object.table == fixture->tables);
    event[4] = 201;
    return fixture->fail_update ? ab_fail(error, "Injected removal update failure")
                                : true;
}

static bool prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_effects_case(sample, fixture->input);
    for (size_t table = 0; table < 4; ++table) {
        fixture->values[table] =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + table};
    }
    const char *const fields[] = {"name", "specialty", "explosionSound",
                                  "killSilently"};
    for (size_t object = 0; object < 2; ++object) {
        for (size_t field = 0; field < 4; ++field) {
            fixture->objects[object][field].key = text(fields[field]);
        }
        fixture->objects[object][0].value = text("body");
        fixture->objects[object][1].value =
            scalar(fixture, fixture->input[object + 1u], "BOMB");
        fixture->objects[object][2].value =
            scalar(fixture, fixture->input[object + 9u], "sound");
        fixture->objects[object][3].value =
            scalar(fixture, fixture->input[4], "truthy");
        fixture->tables[object] = (AbDataTable){fixture->objects[object], 4, 4, NULL};
    }
    fixture->definition =
        (AbDataEntry){text("specialty"), scalar(fixture, fixture->input[3], "BOMB")};
    fixture->tables[2] = (AbDataTable){&fixture->definition, 1, 1, NULL};
    fixture->particles = particle(fixture, fixture->input[5]);
    fixture->amount = scalar(fixture, fixture->input[6], "truthy");
    fixture->sound = scalar(fixture, fixture->input[7], "sound");
    for (size_t map = 0; map < 2; ++map) {
        CHECK(ab_string_map_open(1, 1, 4096, fixture->maps + map, NULL));
    }
    static AbDataValue map_scalar[4] = {{0},
                                        {.kind = AB_DATA_BOOLEAN},
                                        {.kind = AB_DATA_BOOLEAN, .bits = 1},
                                        {.kind = AB_DATA_NUMBER}};
    AbDataValue *first = fixture->input[12] >= 4
                             ? fixture->values + (fixture->input[12] - 4u)
                             : map_scalar + fixture->input[12];
    CHECK(ab_string_map_set(fixture->maps[0], span("queued"), (uintptr_t)first,
                            fixture->input[12] != 0, NULL));
    CHECK(ab_string_map_set(fixture->maps[1], span("queued"),
                            (uintptr_t)(fixture->values + 1), true, NULL));
    fixture->state.dead_blocks =
        fixture->input[11] ? fixture->maps[fixture->input[11] - 1u] : NULL;
    fixture->entry = (AbBlockRemovalEntry){span("queued"), fixture->values[0]};
    uint32_t mask = fixture->input[13];
    fixture->host = (AbBlockRemovalEffectsHost){
        fixture,
        &fixture->state,
        mask & 1u ? property : NULL,
        mask & 2u ? destroyed_sound : NULL,
        mask & 4u ? random_value : NULL,
        mask & 8u ? (fixture->input[21] == 1 ? explosion_first : explosion_second)
                  : NULL,
        mask & 16u ? audio_name : NULL,
        fixture->input[14] == 0   ? NULL
        : fixture->input[14] == 1 ? audio_first
                                  : audio_second,
        mask & 32u ? update : NULL};
    return true;
}

static void close_fixture(Fixture *fixture) {
    for (size_t map = 0; map < 2; ++map) {
        ab_string_map_close(fixture->maps[map]);
    }
}

static bool edges(void) {
    Fixture fixture;
    CHECK(prepare(&fixture, 24));
    fixture.mutate_entry = true;
    fixture.definition.value = text("BOMB");
    fixture.objects[0][1].value = text("BOMB");
    fixture.objects[0][3].value = (AbDataValue){0};
    fixture.sound = text("sound");
    fixture.fail_update = true;
    AbBlockRemovalEffects result = {.particles = text("preserved")};
    CHECK(!ab_block_removal_effects(&fixture.entry, fixture.values[2], &fixture.host,
                                    &result, NULL));
    CHECK(fixture.calls[4] == 1 && fixture.calls[5] == 1 && fixture.calls[6] == 1);
    CHECK(result.particles.kind == AB_DATA_STRING && result.particles.string.size == 9);
    close_fixture(&fixture);
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Removal effects sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_EFFECTS_V1 4096\n"));
    size_t matched = 0, failures = 0, counts[7] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        CHECK(prepare(&fixture, sample));
        CHECK(words(file, fixture.input, 22, sample, "input", &matched));
        AbBlockRemovalEffects output = {0};
        uint32_t result[5] = {0};
        result[0] = !ab_block_removal_effects(&fixture.entry, fixture.values[2],
                                              &fixture.host, &output, NULL);
        if (!result[0]) {
            value_words(&fixture, output.particles, result + 1);
            value_words(&fixture, output.amount, result + 3);
        }
        CHECK(words(file, result, 5, sample, "result", &matched));
        uint32_t state[37];
        snapshot(&fixture, state);
        CHECK(words(file, state, 37, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 57, sample, "event", &matched));
        }
        failures += result[0];
        for (size_t kind = 0; kind < 7; ++kind) {
            counts[kind] += fixture.calls[kind];
        }
        close_fixture(&fixture);
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal effects reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal effect phases match: %zu words, %zu failures, "
           "%zu properties, %zu sounds, %zu random draws, %zu audio queries, %zu "
           "explosions, %zu audio calls and %zu updates.\n",
           matched, failures, counts[0], counts[1], counts[2], counts[3], counts[4],
           counts[5], counts[6]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
