#include "ab/block_removal_lifecycle.h"
#include "ab/block_removal_pass.h"
#include "block_removal_cleanup_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal cleanup failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable tables[5];
    AbDataEntry object[8], container[2], world, dynamic;
    AbDataValue values[5];
    AbStringMap *goals, *dead;
    AbBlockRemovalState removal;
    AbBlockRemovalCleanupState state;
    AbBlockRemovalCleanupHost host;
    AbBlockRemovalEntry entry;
    uint32_t input[15], events[2][49], count, calls[2];
} Fixture;

static const char *const strings[] = {"body", "renamed", "truthy"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind, size_t table) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    return kind == 3   ? (AbDataValue){.kind = AB_DATA_NUMBER}
           : kind == 4 ? text("truthy")
                       : fixture->values[table];
}

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 3; ++index) {
            if (value.string.size == strlen(strings[index]) &&
                !memcmp(value.string.data, strings[index], value.string.size)) {
                output[1] = index + 1u;
                break;
            }
        }
    }
}

static void snapshot(Fixture *fixture, uint32_t output[29]) {
    for (size_t field = 0; field < 5; ++field) {
        value_words(fixture, fixture->object[field].value, output + field * 2u);
    }
    value_words(fixture, fixture->removal.objects, output + 10);
    value_words(fixture, fixture->container[0].value, output + 12);
    value_words(fixture, fixture->container[1].value, output + 14);
    value_words(fixture, fixture->world.value, output + 16);
    value_words(fixture, fixture->dynamic.value, output + 18);
    output[20] = fixture->state.level_goals != NULL;
    output[21] = fixture->removal.dead_blocks != NULL;
    uintptr_t value = 0;
    bool present = false;
    (void)ab_string_map_get(fixture->goals, span("queued"), &value, &present, NULL);
    output[22] = present;
    (void)ab_string_map_get(fixture->dead, span("queued"), &value, &present, NULL);
    output[23] = present;
    value_words(fixture, fixture->state.level_name, output + 24);
    output[26] =
        (fixture->host.remove_pig ? 1u : 0u) | (fixture->host.collect_gift ? 2u : 0u);
    output[27] = fixture->calls[0];
    output[28] = fixture->calls[1];
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t argc) {
    if (fixture->count >= 2) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = kind;
    event[1] = pc;
    event[2] = 0;
    event[3] = argc;
    snapshot(fixture, event + 20);
    ++fixture->calls[kind - 1u];
    return event;
}

static bool remove_pig(void *context, AbSpan key, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 1, 466, 1);
    CHECK(event && key.size == 6 && !memcmp(key.data, "queued", 6));
    event[4] = 1;
    uint32_t mutation = fixture->input[12];
    if (mutation & 1u) {
        fixture->object[2].value = (AbDataValue){0};
    }
    if (mutation & 2u) {
        fixture->object[2].value = scalar(fixture, 2, 4);
    }
    if (mutation & 4u) {
        fixture->state.level_name = text("renamed");
    }
    if (mutation & 8u) {
        fixture->object[3].value = scalar(fixture, 1, 4);
        fixture->object[4].value = fixture->values[4];
    }
    if (mutation & 16u) {
        fixture->host.collect_gift = NULL;
    }
    if (mutation & 32u) {
        fixture->removal.objects = (AbDataValue){0};
    }
    return true;
}

static bool collect_gift(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                         AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 2, 475, 4);
    CHECK(event);
    event[4] = 501;
    value_words(fixture, level, event + 5);
    value_words(fixture, x, event + 7);
    value_words(fixture, y, event + 9);
    uint32_t mutation = fixture->input[14];
    if (mutation & 1u) {
        fixture->object[0].value = text("renamed");
    }
    if (mutation & 2u) {
        fixture->object[1].value = scalar(fixture, 1, 4);
    }
    if (mutation & 4u) {
        fixture->state.level_name = (AbDataValue){0};
    }
    return true;
}

static bool prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_cleanup_case(sample, fixture->input);
    for (size_t table = 0; table < 5; ++table) {
        fixture->values[table] =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + table};
    }
    const char *const fields[] = {"name", "levelGoal", "isGiftBox", "x", "y"};
    for (size_t field = 0; field < 5; ++field) {
        fixture->object[field].key = text(fields[field]);
        fixture->object[field].value =
            field ? scalar(fixture, fixture->input[field + 5u], 4) : text("body");
    }
    fixture->object[5] = (AbDataEntry){text("strength"), {.kind = AB_DATA_NUMBER}};
    fixture->object[6] =
        (AbDataEntry){text("killSilently"), {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    fixture->object[7] = (AbDataEntry){text("definition"), text("raw")};
    fixture->tables[0] = (AbDataTable){fixture->object, 8, 8, NULL};
    fixture->container[0] =
        (AbDataEntry){text("world"), scalar(fixture, fixture->input[2], 2)};
    fixture->container[1] =
        (AbDataEntry){text("dynamic"), scalar(fixture, fixture->input[3], 3)};
    fixture->tables[1] = (AbDataTable){fixture->container, 2, 2, NULL};
    uint32_t present = fixture->input[13];
    fixture->world = (AbDataEntry){text("queued"), present & 1u ? fixture->values[0]
                                                                : (AbDataValue){0}};
    fixture->dynamic = (AbDataEntry){text("queued"), present & 2u ? fixture->values[0]
                                                                  : (AbDataValue){0}};
    fixture->tables[2] = (AbDataTable){&fixture->world, 1, 1, NULL};
    fixture->tables[3] = (AbDataTable){&fixture->dynamic, 1, 1, NULL};
    CHECK(ab_string_map_open(1, 1, 4096, &fixture->goals, NULL) &&
          ab_string_map_open(1, 1, 4096, &fixture->dead, NULL));
    CHECK(ab_string_map_set(fixture->goals, span("queued"), 1, (present & 4u) != 0,
                            NULL));
    CHECK(
        ab_string_map_set(fixture->dead, span("queued"), 1, (present & 8u) != 0, NULL));
    fixture->removal = (AbBlockRemovalState){scalar(fixture, fixture->input[1], 1),
                                             fixture->input[5] ? fixture->dead : NULL};
    fixture->state = (AbBlockRemovalCleanupState){
        &fixture->removal, fixture->input[4] ? fixture->goals : NULL,
        scalar(fixture, fixture->input[10], 4)};
    fixture->host = (AbBlockRemovalCleanupHost){
        fixture, fixture->input[11] & 1u ? remove_pig : NULL,
        fixture->input[11] & 2u ? collect_gift : NULL};
    fixture->entry = (AbBlockRemovalEntry){span("queued"), fixture->values[0]};
    return true;
}

typedef struct {
    Fixture *fixture;
    AbBlockRemovalState desired;
    AbDataValue saved[4];
    AbDataTable world, objects, raw, catalog, blocks;
    AbDataEntry world_entry, object_fields[2], raw_field, catalog_field, blocks_field;
    AbStringMap *dead;
} Prefix;

static bool prefix_noop(void *context, AbDataValue object, AbError *error) {
    Prefix *prefix = context;
    (void)error;
    CHECK(object.table == prefix->fixture->tables);
    return true;
}

static bool prefix_definition(void *context, AbSpan key, AbDataValue *output,
                              AbError *error) {
    Prefix *prefix = context;
    (void)error;
    CHECK(key.size == 6 && !memcmp(key.data, "queued", 6));
    *output = (AbDataValue){.kind = AB_DATA_TABLE, .table = &prefix->raw};
    return true;
}

static bool prefix_score(void *context, AbDataValue object, AbScoreNumber *output,
                         AbError *error) {
    CHECK(prefix_noop(context, object, error));
    *output = (AbScoreNumber){0, true};
    return true;
}

static bool prefix_floating(void *context, const AbBlockRemovalFloatingScore *request,
                            AbError *error) {
    (void)context;
    (void)error;
    CHECK(!request->text_present && request->sprite.kind == AB_DATA_BOOLEAN &&
          !request->sprite.bits && request->score.present && request->score.value == 0);
    return true;
}

static bool prefix_property(void *context, AbDataValue object, AbSpan key,
                            AbDataValue *output, AbError *error) {
    CHECK(prefix_noop(context, object, error));
    CHECK((key.size == 9 && !memcmp(key.data, "particles", 9)) ||
          (key.size == 14 && !memcmp(key.data, "particleAmount", 14)));
    *output = (AbDataValue){0};
    return true;
}

static bool prefix_sound(void *context, AbDataValue object, AbDataValue definition,
                         AbDataValue *output, AbError *error) {
    Prefix *prefix = context;
    CHECK(prefix_noop(context, object, error) && definition.table == &prefix->raw);
    *output = (AbDataValue){0};
    return true;
}

static bool prefix_random(void *context, float lower, float upper, AbDataValue *output,
                          AbError *error) {
    (void)context;
    (void)error;
    CHECK(lower == 12 && upper == 20);
    *output = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x41880000)};
    return true;
}

static bool prefix_functions(void *context, AbDataValue object,
                             AbUiPointerValue *output, AbError *error) {
    CHECK(prefix_noop(context, object, error));
    *output = (AbUiPointerValue){0};
    return true;
}

static bool prefix_restore(void *context, AbSpan key, AbError *error) {
    Prefix *prefix = context;
    (void)error;
    CHECK(key.size == 6 && !memcmp(key.data, "queued", 6));
    prefix->fixture->removal = prefix->desired;
    for (size_t field = 0; field < 4; ++field) {
        prefix->fixture->object[field + 1u].value = prefix->saved[field];
    }
    return true;
}

/* Compose the real C phases through this existing native cleanup projection.
 * Its original earlier hosts are intentionally inert, as in the APK oracle.
 * Earlier mutable/coupled host coverage is a separate full-pass recovery gate. */
static bool composed(Fixture *fixture) {
    Prefix prefix = {.fixture = fixture, .desired = fixture->removal};
    for (size_t field = 0; field < 4; ++field) {
        prefix.saved[field] = fixture->object[field + 1u].value;
        fixture->object[field + 1u].value = (AbDataValue){0};
    }
    prefix.world_entry = (AbDataEntry){text("queued"), fixture->values[0]};
    prefix.world = (AbDataTable){&prefix.world_entry, 1, 1, NULL};
    prefix.object_fields[0] =
        (AbDataEntry){text("world"), {.kind = AB_DATA_TABLE, .table = &prefix.world}};
    prefix.object_fields[1] = prefix.object_fields[0];
    prefix.object_fields[1].key = text("dynamic");
    prefix.objects = (AbDataTable){prefix.object_fields, 2, 2, NULL};
    prefix.raw_field = (AbDataEntry){text("spriteScore"), {.kind = AB_DATA_BOOLEAN}};
    prefix.raw = (AbDataTable){&prefix.raw_field, 1, 1, NULL};
    prefix.catalog_field =
        (AbDataEntry){text("raw"), {.kind = AB_DATA_TABLE, .table = &prefix.raw}};
    prefix.catalog = (AbDataTable){&prefix.catalog_field, 1, 1, NULL};
    prefix.blocks_field = (AbDataEntry){
        text("blocks"), {.kind = AB_DATA_TABLE, .table = &prefix.catalog}};
    prefix.blocks = (AbDataTable){&prefix.blocks_field, 1, 1, NULL};
    CHECK(ab_string_map_open(1, 1, 4096, &prefix.dead, NULL));
    CHECK(ab_string_map_set(prefix.dead, span("queued"), (uintptr_t)(fixture->values),
                            true, NULL));
    fixture->removal = (AbBlockRemovalState){
        {.kind = AB_DATA_TABLE, .table = &prefix.objects}, prefix.dead};
    float individual = 0;
    AbBlockRemovalScores scores = {{0, true}, &individual, 0, 1};
    AbBlockRemovalScoreHost scoring = {&prefix,         prefix_noop, prefix_definition,
                                       prefix_score,    NULL,        NULL,
                                       prefix_floating, {1, true},   &scores};
    AbBlockRemovalEffectsHost effects = {&prefix,
                                         &fixture->removal,
                                         prefix_property,
                                         prefix_sound,
                                         prefix_random,
                                         NULL,
                                         NULL,
                                         NULL,
                                         prefix_noop};
    AbBlockRemovalParticlesHost particles = {
        &prefix, prefix_noop, prefix_functions, NULL, NULL, NULL, NULL};
    AbBlockRemovalDetachHost detach = {
        &prefix, {.kind = AB_DATA_TABLE, .table = &prefix.blocks}, prefix_restore};
    AbBlockRemovalPassHost pass = {NULL,    &scoring, &effects,        &particles,
                                   &detach, NULL,     &fixture->state, &fixture->host};
    bool okay = ab_block_removal_pass(&fixture->removal, &pass, NULL);
    ab_string_map_close(prefix.dead);
    return okay;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Removal cleanup sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file, bool compose) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_CLEANUP_V1 4096\n"));
    size_t matched = 0, failures = 0, pigs = 0, gifts = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        CHECK(prepare(&fixture, sample));
        CHECK(words(file, fixture.input, 15, sample, "input", &matched));
        uint32_t failed =
            compose ? !composed(&fixture)
                    : !ab_block_removal_cleanup(&fixture.entry, &fixture.state,
                                                &fixture.host, NULL);
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        uint32_t state[29];
        snapshot(&fixture, state);
        CHECK(words(file, state, 29, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 49, sample, "event", &matched));
        }
        failures += failed;
        pigs += fixture.calls[0];
        gifts += fixture.calls[1];
        ab_string_map_close(fixture.goals);
        ab_string_map_close(fixture.dead);
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal cleanup reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal cleanup phases match: %zu words, %zu failures, "
           "%zu pig removals and %zu gift collections.\n",
           matched, failures, pigs, gifts);
    if (compose) {
        puts("Compared through the complete C removal pass with controlled inert "
             "earlier hosts.");
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && !strcmp(argv[2], "--pass"))) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file, argc == 3);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
