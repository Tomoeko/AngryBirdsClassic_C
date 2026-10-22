#include "ab/block_removal_particles.h"
#include "block_removal_particles_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal particles failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable tables[4];
    AbDataEntry object_fields[2], removal;
    AbDataValue values[4], use_new, new_particles;
    AbUiPointerValue binding, items[2][4];
    AbBlockRemovalFunctions lists[2];
    AbBlockRemovalEntry entry;
    AbBlockRemovalEffects cached;
    AbBlockRemovalParticlesHost host;
    uint32_t input[17], events[8][56], count, calls[5];
    bool fail_old, mutate_entry;
} Fixture;

static const char *const strings[] = {"body",       "renamed", "old", "new",
                                      "descriptor", "truthy",  "list"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind, const char *string,
                          size_t table) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    return kind == 3   ? (AbDataValue){.kind = AB_DATA_NUMBER}
           : kind == 4 ? text(string)
                       : fixture->values[table];
}

static AbUiPointerValue list_binding(Fixture *fixture, uint32_t kind) {
    if (!kind) {
        return (AbUiPointerValue){0};
    }
    if (kind == 1) {
        return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    }
    if (kind == 2) {
        return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER};
    }
    return kind == 5
               ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span("list")}
               : (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                    .object =
                                        (uintptr_t)(fixture->lists + (kind - 3u))};
}

static AbUiPointerValue item(uint32_t kind) {
    if (!kind) {
        return (AbUiPointerValue){0};
    }
    if (kind <= 2) {
        return (AbUiPointerValue){.kind = kind == 1 ? AB_UI_POINTER_BOOLEAN
                                                    : AB_UI_POINTER_NUMBER};
    }
    return (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = kind - 2u};
}

static void data_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t string = 0; string < 7; ++string) {
            if (value.string.size == strlen(strings[string]) &&
                !memcmp(value.string.data, strings[string], value.string.size)) {
                output[1] = string + 1u;
                break;
            }
        }
    }
}

static void pointer_words(Fixture *fixture, AbUiPointerValue value,
                          uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5, 6};
    output[0] = types[value.kind];
    output[1] =
        value.kind == AB_UI_POINTER_BOOLEAN    ? (uint32_t)value.boolean
        : value.kind == AB_UI_POINTER_NUMBER   ? value.number_bits
        : value.kind == AB_UI_POINTER_TEXT     ? 7u
        : value.kind == AB_UI_POINTER_FUNCTION ? (uint32_t)value.function
        : value.kind == AB_UI_POINTER_OBJECT
            ? (uint32_t)((AbBlockRemovalFunctions *)value.object - fixture->lists) +
                  301u
            : 0;
}

static bool notify(void *context, AbDataValue object, AbError *error);
static bool functions(void *context, AbDataValue object, AbUiPointerValue *output,
                      AbError *error);
static bool call(void *context, AbUiPointerValue function, AbDataValue object,
                 AbError *error);
static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error);
static bool new_first(void *context, AbSpan key, AbDataValue descriptor, bool flag1,
                      bool flag2, AbError *error);
static bool new_second(void *context, AbSpan key, AbDataValue descriptor, bool flag1,
                       bool flag2, AbError *error);
static bool old_first(void *context, AbSpan key, AbDataValue particles,
                      AbDataValue amount, bool flag1, bool flag2, AbError *error);
static bool old_second(void *context, AbSpan key, AbDataValue particles,
                       AbDataValue amount, bool flag1, bool flag2, AbError *error);

static void snapshot(Fixture *fixture, uint32_t output[36]) {
    data_words(fixture, fixture->object_fields[0].value, output);
    data_words(fixture, fixture->object_fields[1].value, output + 2);
    pointer_words(fixture, fixture->binding, output + 4);
    for (size_t list = 0; list < 2; ++list) {
        for (size_t slot = 0; slot < 4; ++slot) {
            pointer_words(fixture, fixture->items[list][slot],
                          output + 6 + list * 8u + slot * 2u);
        }
    }
    data_words(fixture, fixture->use_new, output + 22);
    data_words(fixture, fixture->new_particles, output + 24);
    data_words(fixture, fixture->removal.value, output + 26);
    output[28] = (fixture->host.notify ? 1u : 0u) | (fixture->host.property ? 2u : 0u) |
                 (fixture->host.old_particles ? 4u : 0u) |
                 (fixture->host.new_particles ? 8u : 0u);
    output[29] = !fixture->host.new_particles               ? 0u
                 : fixture->host.new_particles == new_first ? 1u
                                                            : 2u;
    output[30] = !fixture->host.old_particles               ? 0u
                 : fixture->host.old_particles == old_first ? 1u
                                                            : 2u;
    memcpy(output + 31, fixture->calls, sizeof(fixture->calls));
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t variant,
                        uint32_t argc) {
    if (fixture->count >= 8) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    memset(event, 0, 56 * sizeof(*event));
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    snapshot(fixture, event + 20);
    ++fixture->calls[kind - 1u];
    return event;
}

static bool notify(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 1, 233, 0, 1);
    CHECK(event && object.table == fixture->tables);
    event[4] = 201;
    uint32_t mutation = fixture->input[11];
    if (mutation & 1u) {
        fixture->binding = list_binding(fixture, 4);
    }
    if (mutation & 2u) {
        fixture->object_fields[1].value = scalar(fixture, 1, "", 3);
    }
    if (mutation & 4u) {
        fixture->use_new = scalar(fixture, 2, "", 3);
    }
    if (mutation & 8u) {
        fixture->host.property = NULL;
    }
    if (mutation & 16u) {
        fixture->host.new_particles = NULL;
    }
    if (mutation & 32u) {
        fixture->host.old_particles = NULL;
    }
    if (mutation & 64u) {
        fixture->object_fields[0].value = text("renamed");
    }
    if (fixture->mutate_entry) {
        fixture->entry.key = span("renamed");
        fixture->entry.object = (AbDataValue){0};
        fixture->cached.particles = text("changed");
    }
    return true;
}

static bool functions(void *context, AbDataValue object, AbUiPointerValue *output,
                      AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == fixture->tables);
    *output = fixture->binding;
    return true;
}

static bool call(void *context, AbUiPointerValue function, AbDataValue object,
                 AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 2, 244, (uint32_t)function.function, 1);
    CHECK(event && object.table == fixture->tables);
    event[4] = 201;
    uint32_t next = fixture->calls[1], mutation = fixture->input[12];
    if (next < 4 && (mutation & 1u)) {
        fixture->items[0][next] = item(4);
    }
    if (next < 4 && (mutation & 2u)) {
        fixture->items[0][next] = item(0);
    }
    if (mutation & 4u) {
        fixture->binding = list_binding(fixture, 4);
    }
    if (mutation & 8u) {
        fixture->object_fields[1].value = scalar(fixture, 2, "", 3);
    }
    if (mutation & 16u) {
        fixture->object_fields[1].value = scalar(fixture, 1, "", 3);
    }
    if (mutation & 32u) {
        fixture->host.new_particles = NULL;
    }
    if (mutation & 64u) {
        fixture->use_new = scalar(fixture, 2, "", 3);
    }
    return true;
}

static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error) {
    Fixture *fixture = context;
    (void)error;
    bool use = key.size == 19 && !memcmp(key.data, "useNewParticleTable", 19);
    CHECK(use || (key.size == 9 && !memcmp(key.data, "particles", 9)));
    uint32_t *event = record(fixture, 3, use ? 253u : 261u, 0, 2);
    CHECK(event && object.table == fixture->tables);
    event[4] = 501;
    event[5] = use ? 1u : 2u;
    uint32_t mutation = fixture->input[use ? 13 : 14];
    if (mutation & 1u) {
        fixture->host.new_particles = new_second;
    }
    if (mutation & 2u) {
        if (use) {
            fixture->new_particles = (AbDataValue){0};
        } else {
            fixture->removal.value = scalar(fixture, 1, "", 3);
        }
    }
    if ((use && (mutation & 4u)) || (!use && (mutation & 8u))) {
        fixture->object_fields[1].value = scalar(fixture, 2, "", 3);
    }
    if (use && (mutation & 8u)) {
        fixture->object_fields[0].value = text("renamed");
    }
    if ((use && (mutation & 16u)) || (!use && (mutation & 4u))) {
        fixture->host.new_particles = NULL;
    }
    *output = use ? fixture->use_new : fixture->new_particles;
    return true;
}

static bool new_particles(void *context, AbSpan key, AbDataValue descriptor, bool flag1,
                          bool flag2, uint32_t variant, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 4, 265, variant, 4);
    CHECK(event && key.size == 6 && !memcmp(key.data, "queued", 6) && !flag1 && !flag2);
    event[4] = 1;
    data_words(fixture, descriptor, event + 6);
    event[8] = event[10] = 1;
    return true;
}

static bool new_first(void *context, AbSpan key, AbDataValue descriptor, bool flag1,
                      bool flag2, AbError *error) {
    return new_particles(context, key, descriptor, flag1, flag2, 1, error);
}

static bool new_second(void *context, AbSpan key, AbDataValue descriptor, bool flag1,
                       bool flag2, AbError *error) {
    return new_particles(context, key, descriptor, flag1, flag2, 2, error);
}

static bool old_particles(void *context, AbSpan key, AbDataValue particles,
                          AbDataValue amount, bool flag1, bool flag2, uint32_t variant,
                          AbError *error) {
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 5, 273, variant, 5);
    CHECK(event && key.size == 6 && !memcmp(key.data, "queued", 6) && !flag1 && !flag2);
    event[4] = 1;
    data_words(fixture, particles, event + 6);
    data_words(fixture, amount, event + 8);
    event[10] = event[12] = 1;
    return fixture->fail_old ? ab_fail(error, "Injected old-particle failure") : true;
}

static bool old_first(void *context, AbSpan key, AbDataValue particles,
                      AbDataValue amount, bool flag1, bool flag2, AbError *error) {
    return old_particles(context, key, particles, amount, flag1, flag2, 1, error);
}

static bool old_second(void *context, AbSpan key, AbDataValue particles,
                       AbDataValue amount, bool flag1, bool flag2, AbError *error) {
    return old_particles(context, key, particles, amount, flag1, flag2, 2, error);
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_particles_case(sample, fixture->input);
    for (size_t table = 0; table < 4; ++table) {
        fixture->values[table] =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + table};
    }
    fixture->object_fields[0] = (AbDataEntry){text("name"), text("body")};
    fixture->object_fields[1] = (AbDataEntry){
        text("killSilently"), scalar(fixture, fixture->input[1], "truthy", 3)};
    fixture->tables[0] = (AbDataTable){fixture->object_fields, 2, 2, NULL};
    fixture->removal = (AbDataEntry){
        text("removal"), scalar(fixture, fixture->input[9], "descriptor", 3)};
    fixture->tables[2] = (AbDataTable){&fixture->removal, 1, 1, NULL};
    for (size_t list = 0; list < 2; ++list) {
        fixture->lists[list] = (AbBlockRemovalFunctions){fixture->items[list], 4};
        for (uint32_t slot = 0; slot < 4; ++slot) {
            fixture->items[list][slot] =
                item((fixture->input[3 + list] >> (slot * 3u)) & 7u);
        }
    }
    fixture->binding = list_binding(fixture, fixture->input[2]);
    fixture->use_new = scalar(fixture, fixture->input[5], "truthy", 3);
    fixture->new_particles = scalar(fixture, fixture->input[8], "new", 2);
    fixture->entry = (AbBlockRemovalEntry){span("queued"), fixture->values[0]};
    fixture->cached.particles = scalar(fixture, fixture->input[6], "old", 3);
    fixture->cached.amount =
        fixture->input[7] <= 1
            ? (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x41400000)}
            : scalar(fixture, fixture->input[7], "truthy", 3);
    uint32_t mask = fixture->input[10];
    fixture->host = (AbBlockRemovalParticlesHost){
        fixture,
        mask & 1u ? notify : NULL,
        functions,
        call,
        mask & 2u ? property : NULL,
        mask & 8u ? (fixture->input[15] == 1 ? new_first : new_second) : NULL,
        mask & 4u ? (fixture->input[16] == 1 ? old_first : old_second) : NULL};
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 0);
    fixture.binding = (AbUiPointerValue){0};
    fixture.use_new = (AbDataValue){0};
    fixture.object_fields[1].value = (AbDataValue){0};
    fixture.mutate_entry = fixture.fail_old = true;
    CHECK(!ab_block_removal_particles(&fixture.entry, &fixture.cached, &fixture.host,
                                      NULL));
    CHECK(fixture.calls[0] == 1 && fixture.calls[4] == 1);
    CHECK(fixture.events[2][6] == 0);
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Removal particles sample %u %s[%zu]: C=%08x original=%08x\n",
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
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_PARTICLES_V1 4096\n"));
    size_t matched = 0, failures = 0, counts[5] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 17, sample, "input", &matched));
        uint32_t failed = !ab_block_removal_particles(&fixture.entry, &fixture.cached,
                                                      &fixture.host, NULL);
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        uint32_t state[36];
        snapshot(&fixture, state);
        CHECK(words(file, state, 36, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 56, sample, "event", &matched));
        }
        failures += failed;
        for (size_t kind = 0; kind < 5; ++kind) {
            counts[kind] += fixture.calls[kind];
        }
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal particles reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal particle phases match: %zu words, %zu failures, "
           "%zu notifications, %zu functions, %zu properties, %zu new and %zu legacy "
           "particle requests.\n",
           matched, failures, counts[0], counts[1], counts[2], counts[3], counts[4]);
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
