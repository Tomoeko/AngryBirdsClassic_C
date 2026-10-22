#include "ab/block_removal_score.h"
#include "block_removal_score_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal score failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable tables[4];
    AbDataEntry object_fields[4], definition_fields[2][2];
    AbDataValue current_definition, returned_definition;
    AbScoreNumber increment;
    AbBlockRemovalScores scores;
    AbBlockRemovalScoreHost host;
    AbBlockRemovalEntry entry;
    float score_entry;
    uint32_t input[25], events[6][57], count, calls[6], destroy_kind;
    bool fail_float, mutate_entry;
} Fixture;

static const char *const strings[] = {"body",  "renamed", "glyph", "changed",
                                      "sound", "audio",   "truthy"};

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

static float number(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static AbDataValue table(Fixture *fixture, size_t index) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + index};
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind, const char *string) {
    if (kind == 0) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    return kind == 3   ? (AbDataValue){.kind = AB_DATA_NUMBER}
           : kind == 4 ? text(string)
                       : table(fixture, 3);
}

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 7; ++index) {
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

static void score_words(AbScoreNumber value, uint32_t output[2]) {
    output[0] = value.present ? 3u : 0u;
    output[1] = value.present ? bits(value.value) : 0;
}

static bool destroy(void *context, AbDataValue object, AbError *error);
static bool definition(void *context, AbSpan key, AbDataValue *output, AbError *error);
static bool score(void *context, AbDataValue object, AbScoreNumber *output,
                  AbError *error);
static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
static bool audio_first(void *context, AbAudioPlay request, AbError *error);
static bool audio_second(void *context, AbAudioPlay request, AbError *error);
static bool floating(void *context, const AbBlockRemovalFloatingScore *request,
                     AbError *error);

static void snapshot(Fixture *fixture, uint32_t output[37]) {
    for (size_t field = 0; field < 4; ++field) {
        value_words(fixture, fixture->object_fields[field].value, output + field * 2u);
    }
    for (size_t object = 0; object < 2; ++object) {
        for (size_t field = 0; field < 2; ++field) {
            value_words(fixture, fixture->definition_fields[object][field].value,
                        output + 8 + object * 4u + field * 2u);
        }
    }
    value_words(fixture, fixture->current_definition, output + 16);
    score_words(fixture->increment, output + 18);
    score_words(fixture->host.floating_scaling, output + 20);
    score_words(fixture->scores.total, output + 22);
    output[24] = fixture->scores.entries != NULL;
    output[25] = (uint32_t)fixture->scores.count;
    output[26] = bits(fixture->score_entry);
    output[27] = fixture->host.scores != NULL;
    output[28] = !fixture->host.audio                 ? 0u
                 : fixture->host.audio == audio_first ? 1u
                                                      : 2u;
    output[29] =
        (fixture->host.definition ? 1u : 0u) | (fixture->host.score ? 2u : 0u) |
        (fixture->host.audio_name ? 4u : 0u) | (fixture->host.floating ? 8u : 0u);
    output[30] = fixture->destroy_kind;
    memcpy(output + 31, fixture->calls, sizeof(fixture->calls));
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t variant,
                        uint32_t argc) {
    if (fixture->count >= 6) {
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

static bool destroy(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 1, 45, 0, 1);
    CHECK(event && object.kind == AB_DATA_TABLE && object.table == fixture->tables);
    event[4] = 501;
    uint32_t mutation = fixture->input[16];
    if (mutation & 1u) {
        fixture->object_fields[0].value = text("renamed");
    }
    if (mutation & 2u) {
        fixture->object_fields[1].value = scalar(fixture, 1, "truthy");
    }
    if (mutation & 4u) {
        fixture->host.score = NULL;
    }
    if (mutation & 8u) {
        fixture->host.definition = NULL;
    }
    if (mutation & 16u) {
        fixture->definition_fields[0][0].value = text("changed");
    }
    if (mutation & 32u) {
        fixture->current_definition = table(fixture, 2);
    }
    if (mutation & 64u) {
        fixture->object_fields[2].value = scalar(fixture, 2, "truthy");
    }
    if (mutation & 128u) {
        fixture->host.floating_scaling =
            (AbScoreNumber){number(fixture->input[20]), true};
    }
    if (fixture->mutate_entry) {
        fixture->entry.key = span("renamed");
        fixture->entry.object = (AbDataValue){0};
    }
    return true;
}

static bool definition(void *context, AbSpan key, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 2, 48, 0, 1);
    CHECK(event && key.size == 6 && !memcmp(key.data, "queued", 6));
    event[4] = 1;
    if (fixture->input[16] & 256u) {
        fixture->increment.present = false;
    }
    if (fixture->input[16] & 512u) {
        fixture->object_fields[1].value = scalar(fixture, 2, "truthy");
    }
    fixture->returned_definition = fixture->current_definition;
    *output = fixture->returned_definition;
    return true;
}

static bool score(void *context, AbDataValue object, AbScoreNumber *output,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 3, 51, 0, 1);
    CHECK(event && object.kind == AB_DATA_TABLE && object.table == fixture->tables);
    event[4] = 201;
    uint32_t mutation = fixture->input[24];
    if (mutation & 1u) {
        fixture->object_fields[1].value = scalar(fixture, 1, "truthy");
    }
    if (mutation & 2u) {
        fixture->object_fields[1].value = scalar(fixture, 2, "truthy");
    }
    if (mutation & 4u) {
        fixture->definition_fields[0][1].value = scalar(fixture, 1, "truthy");
    }
    if (mutation & 8u) {
        fixture->definition_fields[0][0].value = (AbDataValue){0};
    }
    if (mutation & 16u) {
        fixture->definition_fields[1][0].value = text("changed");
    }
    if (mutation & 32u) {
        fixture->host.floating_scaling =
            (AbScoreNumber){number(fixture->input[20]), true};
    }
    if (mutation & 64u) {
        fixture->scores.total = (AbScoreNumber){number(fixture->input[21]), true};
    }
    if (mutation & 128u) {
        fixture->current_definition = table(fixture, 2);
    }
    *output = fixture->increment;
    return true;
}

static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 4, 61, 0, 1);
    CHECK(event);
    value_words(fixture, sound, event + 4);
    uint32_t mutation = fixture->input[17];
    if (mutation & 1u) {
        fixture->host.audio = audio_second;
    }
    if (mutation & 2u) {
        fixture->object_fields[2].value = table(fixture, 3);
        fixture->object_fields[3].value = scalar(fixture, 1, "truthy");
    }
    if (mutation & 4u) {
        CHECK(fixture->returned_definition.kind == AB_DATA_TABLE);
        fixture
            ->definition_fields
                [fixture->returned_definition.table == fixture->tables + 1 ? 0 : 1][0]
            .value = text("changed");
    }
    if (mutation & 8u) {
        fixture->host.floating_scaling.present = false;
    }
    if (mutation & 16u) {
        fixture->host.floating = NULL;
    }
    if (mutation & 32u) {
        fixture->scores.total = (AbScoreNumber){number(fixture->input[21]), true};
    }
    if (mutation & 64u) {
        fixture->object_fields[1].value = scalar(fixture, 1, "truthy");
    }
    if (mutation & 128u) {
        fixture->host.audio = NULL;
    }
    *output = scalar(fixture, fixture->input[14], "audio");
    return true;
}

static bool audio(void *context, AbAudioPlay request, uint32_t variant,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 5, 64, variant, 3);
    CHECK(event);
    const AbDataValue fields[] = {request.name, request.volume, request.loop,
                                  request.channel};
    for (size_t field = 0; field < 4; ++field) {
        value_words(fixture, fields[field], event + 4 + field * 2u);
    }
    return true;
}

static bool audio_first(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 1, error);
}

static bool audio_second(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 2, error);
}

static uint32_t text_hash(AbSpan value) {
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < value.size; ++index) {
        hash = (hash ^ value.data[index]) * UINT32_C(16777619);
    }
    return hash;
}

static bool floating(void *context, const AbBlockRemovalFloatingScore *request,
                     AbError *error) {
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 6, 0, 0, 1);
    CHECK(event);
    value_words(fixture, request->x, event + 4);
    value_words(fixture, request->y, event + 6);
    value_words(fixture, request->sprite, event + 8);
    event[10] = request->text_present ? 4u : 0u;
    event[11] = request->text_present ? text_hash(request->text) : 0;
    event[12] = request->text_present ? (uint32_t)request->text.size : 0;
    score_words(request->score, event + 13);
    event[15] = bits(request->time);
    event[16] = bits(request->lifetime);
    event[17] = bits(request->max_scale);
    event[18] = bits(request->xs);
    uint32_t mutation = fixture->input[18];
    if (mutation & 1u) {
        fixture->scores.total = (AbScoreNumber){number(fixture->input[21]), true};
    }
    if (mutation & 2u) {
        fixture->scores.total.present = false;
    }
    if (mutation & 4u) {
        fixture->scores.entries = NULL;
    }
    if (mutation & 8u) {
        fixture->host.scores = NULL;
    }
    if (mutation & 16u) {
        fixture->object_fields[0].value = text("renamed");
    }
    if (mutation & 32u) {
        fixture->host.floating_scaling =
            (AbScoreNumber){number(fixture->input[20]), true};
    }
    if (mutation & 64u) {
        fixture->definition_fields[0][0].value = text("changed");
    }
    return fixture->fail_float ? ab_fail(error, "Injected removal floating failure")
                               : true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_score_case(sample, fixture->input);
    fixture->object_fields[0] = (AbDataEntry){text("name"), text("body")};
    fixture->object_fields[1] =
        (AbDataEntry){text("levelGoal"), scalar(fixture, fixture->input[1], "truthy")};
    fixture->object_fields[2] =
        (AbDataEntry){text("x"), scalar(fixture, fixture->input[12], "truthy")};
    fixture->object_fields[3] =
        (AbDataEntry){text("y"), scalar(fixture, fixture->input[13], "truthy")};
    fixture->tables[0] = (AbDataTable){fixture->object_fields, 4, 4, NULL};
    for (size_t definition_index = 0; definition_index < 2; ++definition_index) {
        fixture->definition_fields[definition_index][0] = (AbDataEntry){
            text("spriteScore"), scalar(fixture, fixture->input[3], "glyph")};
        fixture->definition_fields[definition_index][1] = (AbDataEntry){
            text("damageSound"), scalar(fixture, fixture->input[4], "sound")};
        fixture->tables[definition_index + 1] =
            (AbDataTable){fixture->definition_fields[definition_index], 2, 2, NULL};
    }
    fixture->current_definition = fixture->input[2] >= 5
                                      ? table(fixture, fixture->input[2] - 4u)
                                      : scalar(fixture, fixture->input[2], "truthy");
    fixture->increment =
        (AbScoreNumber){number(fixture->input[6]), fixture->input[5] != 0};
    fixture->scores =
        (AbBlockRemovalScores){{number(fixture->input[10]), fixture->input[9] != 0},
                               fixture->input[11] ? &fixture->score_entry : NULL,
                               0,
                               1};
    fixture->entry = (AbBlockRemovalEntry){span("queued"), table(fixture, 0)};
    fixture->destroy_kind = fixture->input[23];
    uint32_t mask = fixture->input[22];
    fixture->host =
        (AbBlockRemovalScoreHost){fixture,
                                  fixture->destroy_kind == 3 ? destroy : NULL,
                                  mask & 1u ? definition : NULL,
                                  mask & 2u ? score : NULL,
                                  mask & 4u ? audio_name : NULL,
                                  fixture->input[15] == 0   ? NULL
                                  : fixture->input[15] == 1 ? audio_first
                                                            : audio_second,
                                  mask & 8u ? floating : NULL,
                                  {number(fixture->input[8]), fixture->input[7] != 0},
                                  &fixture->scores};
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 16);
    fixture.input[16] = fixture.input[17] = fixture.input[18] = fixture.input[24] = 0;
    fixture.increment = (AbScoreNumber){17, true};
    fixture.scores.entries = NULL;
    fixture.scores.total = (AbScoreNumber){2, true};
    fixture.mutate_entry = true;
    AbBlockRemovalScored output = {.definition = text("preserved")};
    CHECK(!ab_block_removal_score(&fixture.entry, &fixture.host, &output, NULL));
    CHECK(fixture.scores.total.value == 19 && fixture.calls[5] == 1 &&
          !fixture.scores.count);
    CHECK(output.definition.kind == AB_DATA_STRING);
    prepare(&fixture, 16);
    fixture.fail_float = true;
    fixture.input[18] = 1;
    CHECK(!ab_block_removal_score(&fixture.entry, &fixture.host, &output, NULL));
    CHECK(bits(fixture.scores.total.value) == fixture.input[21]);
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Removal score sample %u %s[%zu]: C=%08x original=%08x\n",
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
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_SCORE_V1 4096\n"));
    size_t matched = 0, failures = 0, counts[6] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 25, sample, "input", &matched));
        AbBlockRemovalScored output = {0};
        uint32_t result[5] = {0};
        result[0] =
            !ab_block_removal_score(&fixture.entry, &fixture.host, &output, NULL);
        if (!result[0]) {
            value_words(&fixture, output.definition, result + 1);
            score_words(output.increment, result + 3);
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
        for (size_t kind = 0; kind < 6; ++kind) {
            counts[kind] += fixture.calls[kind];
        }
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal score reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal scoring phases match: %zu words, %zu failures, "
           "%zu destroys, %zu definitions, %zu scores, %zu audio queries, %zu audio "
           "calls and %zu floating requests.\n",
           matched, failures, counts[0], counts[1], counts[2], counts[3], counts[4],
           counts[5]);
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
