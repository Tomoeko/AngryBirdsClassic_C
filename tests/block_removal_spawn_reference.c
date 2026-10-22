#include "ab/block_removal_spawn.h"
#include "block_removal_spawn_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Spawn reference failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable body, spawn, death, target, catalog, blocks, types[2];
    AbDataEntry body_fields[6], spawn_fields[2], death_fields[2], target_fields[2];
    AbDataEntry catalog_fields[2], blocks_field, type_fields[2][3];
    AbBlockRemovalEntry entry;
    AbBlockRemovalDetached detached;
    AbBlockRemovalSpawnHost host;
    uint8_t names[80];
    uint32_t input[15], events[20][8], count, creates, rotations, randoms, phase;
} Fixture;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue numeric_word(uint32_t word) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = word};
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static uint32_t string_id(AbDataValue value) {
    if (value.kind == AB_DATA_NIL) {
        return 0;
    }
    return value.string.size == 6 && !memcmp(value.string.data, "target", 6) ? 1u : 2u;
}

static uint32_t hash(AbSpan value) {
    uint32_t output = UINT32_C(2166136261);
    for (size_t index = 0; index < value.size; ++index) {
        output = (output ^ value.data[index]) * UINT32_C(16777619);
    }
    return output;
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc,
                        uint32_t variant) {
    if (fixture->count >= 20) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    return event;
}

static bool rotate_second(void *context, AbDataValue object, float angle,
                          AbError *error);

static bool random_value(void *context, unsigned count, float lower, float upper,
                         AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 3, count ? 424u : 443u, 0);
    CHECK(event);
    event[3] = count;
    event[4] = bits(lower);
    event[5] = bits(upper);
    ++fixture->randoms;
    uint32_t flags = fixture->input[1];
    if (count) {
        CHECK(count == 2 && lower == 1 && upper == (float)fixture->input[14]);
        if (flags & 8u) {
            fixture->death_fields[1].value = table(fixture->types + 1);
        }
        *output = numeric_word(fixture->input[10]);
    } else {
        if (flags & 8u) {
            fixture->host.rotate = rotate_second;
            fixture->host.scale_factor = numeric_word(0x3f000000);
            fixture->host.math_pi = 1;
        }
        if (flags & 64u) {
            fixture->host.create = NULL;
        }
        *output = numeric_word(fixture->input[11]);
    }
    return true;
}

static bool create(void *context, AbDataValue block_table, AbDataValue definition,
                   AbSpan name, float x, float y, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(block_table.kind == AB_DATA_TABLE && block_table.table == &fixture->blocks);
    fixture->phase = name.size > 12u;
    uint32_t *event = record(fixture, 1, fixture->phase ? 437u : 400u, 0);
    CHECK(event);
    event[3] = string_id(definition);
    event[4] = hash(name);
    event[5] = (uint32_t)name.size;
    event[6] = bits(x);
    event[7] = bits(y);
    *output = numeric_word(bits((float)fixture->creates + 100));
    ++fixture->creates;
    uint32_t flags = fixture->input[1];
    if ((flags & 4u) && fixture->creates == 1) {
        fixture->body_fields[0].value = text("renamed");
        fixture->body_fields[1].value = numeric_word(0x41b00000);
        fixture->body_fields[3].value = numeric_word(0x3fc90fdb);
        fixture->body_fields[4].value = numeric_word(0x41f00000);
        fixture->body_fields[5].value = numeric_word(0x42200000);
        fixture->spawn_fields[0].value = text("alternate");
    }
    if (flags & 16u) {
        fixture->host.rotate = NULL;
    }
    if (flags & 32u) {
        fixture->host.random = NULL;
    }
    return true;
}

static bool rotate(void *context, AbDataValue created, float angle, uint32_t variant,
                   AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 2, fixture->phase ? 449u : 409u, variant);
    CHECK(event && created.kind == AB_DATA_NUMBER);
    event[3] = created.bits;
    event[4] = bits(angle);
    ++fixture->rotations;
    return true;
}

static bool rotate_first(void *context, AbDataValue created, float angle,
                         AbError *error) {
    return rotate(context, created, angle, 1, error);
}

static bool rotate_second(void *context, AbDataValue created, float angle,
                          AbError *error) {
    return rotate(context, created, angle, 2, error);
}

static void snapshot(Fixture *fixture, uint32_t output[16]) {
    output[0] = fixture->body_fields[0].value.string.size == 4 ? 1u : 2u;
    for (size_t field = 1; field < 6; ++field) {
        output[field] = fixture->body_fields[field].value.bits;
    }
    output[6] = string_id(fixture->spawn_fields[0].value);
    output[7] = fixture->death_fields[1].value.table == fixture->types ? 1u : 2u;
    output[8] = fixture->host.scale_factor.bits;
    output[9] = bits(fixture->host.math_pi);
    output[10] = fixture->host.create != NULL;
    output[11] = !fixture->host.rotate                  ? 0u
                 : fixture->host.rotate == rotate_first ? 1u
                                                        : 2u;
    output[12] = fixture->host.random != NULL;
    output[13] = fixture->creates;
    output[14] = fixture->rotations;
    output[15] = fixture->randoms;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_spawn_case(sample, fixture->input);
    const char *const fields[] = {"name", "width", "height", "angle", "x", "y"};
    const uint32_t offsets[] = {0, 2, 3, 5, 6, 7};
    for (size_t field = 0; field < 6; ++field) {
        fixture->body_fields[field] = (AbDataEntry){
            text(fields[field]),
            field ? numeric_word(fixture->input[offsets[field]]) : text("body")};
    }
    fixture->body = (AbDataTable){fixture->body_fields, 6, 6, NULL};
    fixture->spawn_fields[0] = (AbDataEntry){text("definition"), text("target")};
    fixture->spawn_fields[1] =
        (AbDataEntry){text("amount"), numeric_word(fixture->input[8])};
    fixture->spawn = (AbDataTable){fixture->spawn_fields, 2, 2, NULL};
    fixture->target_fields[0] =
        (AbDataEntry){text("width"), numeric_word(fixture->input[4])};
    fixture->target_fields[1] = (AbDataEntry){text("height"), {0}};
    fixture->target = (AbDataTable){fixture->target_fields, 2, 2, NULL};
    fixture->catalog_fields[0] = (AbDataEntry){text("target"), table(&fixture->target)};
    fixture->catalog_fields[1] =
        (AbDataEntry){text("alternate"), table(&fixture->target)};
    fixture->catalog = (AbDataTable){fixture->catalog_fields, 2, 2, NULL};
    fixture->blocks_field = (AbDataEntry){text("blocks"), table(&fixture->catalog)};
    fixture->blocks = (AbDataTable){&fixture->blocks_field, 1, 1, NULL};
    for (size_t type = 0; type < 2; ++type) {
        for (uint32_t slot = 0; slot < 3; ++slot) {
            fixture->type_fields[type][slot] =
                (AbDataEntry){numeric_word(bits((float)slot + 1)),
                              text((type + slot) % 2u ? "alternate" : "target")};
        }
        fixture->types[type] =
            (AbDataTable){fixture->type_fields[type], fixture->input[14], 3, NULL};
    }
    fixture->death_fields[0] =
        (AbDataEntry){text("amount"), numeric_word(fixture->input[9])};
    fixture->death_fields[1] = (AbDataEntry){text("types"), table(fixture->types)};
    fixture->death = (AbDataTable){fixture->death_fields, 2, 2, NULL};
    fixture->entry =
        (AbBlockRemovalEntry){{(const uint8_t *)"queued", 6}, table(&fixture->body)};
    uint32_t flags = fixture->input[1];
    AbDataValue grid = flags & 256u ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                       : flags & 1u ? table(&fixture->spawn)
                                    : (AbDataValue){0};
    AbDataValue death = flags & 512u ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                        : flags & 2u ? table(&fixture->death)
                                     : (AbDataValue){0};
    fixture->detached = (AbBlockRemovalDetached){grid, death, numeric_word(0x40e00000),
                                                 numeric_word(0xc0400000)};
    fixture->host = (AbBlockRemovalSpawnHost){fixture,
                                              table(&fixture->blocks),
                                              numeric_word(fixture->input[12]),
                                              number(fixture->input[13]),
                                              fixture->names,
                                              sizeof(fixture->names),
                                              5,
                                              random_value,
                                              flags & 128u ? NULL : create,
                                              rotate_first};
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Removal spawn sample %u %s[%zu]: C=%08x original=%08x\n",
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
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_SPAWN_V1 2048\n"));
    size_t matched = 0, failures = 0, creates = 0, rotates = 0, randoms = 0;
    for (uint32_t sample = 0; sample < 2048; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 15, sample, "input", &matched));
        uint32_t failed = !ab_block_removal_spawn(&fixture.entry, &fixture.detached,
                                                  &fixture.host, NULL);
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        uint32_t state[16];
        snapshot(&fixture, state);
        CHECK(words(file, state, 16, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 8, sample, "event", &matched));
        }
        failures += failed;
        creates += fixture.creates;
        rotates += fixture.rotations;
        randoms += fixture.randoms;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal spawn reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 2,048 original removal spawn phases match: %zu words, %zu failures, "
           "%zu creates, %zu rotations and %zu random draws.\n",
           matched, failures, creates, rotates, randoms);
    return true;
}

int main(int argc, char **argv) {
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
