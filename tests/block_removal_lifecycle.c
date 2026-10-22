#include "ab/block_removal_lifecycle.h"
#include "block_removal_detach_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal lifecycle failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable tables[5];
    AbDataEntry object[4], raw[2], blocks, catalog;
    AbDataValue values[5];
    AbBlockRemovalEntry entry;
    AbBlockRemovalDetachHost host;
    uint32_t input[11], event[36], count;
    bool fail_remove, mutate_entry;
} Fixture;

static const char *const strings[] = {"body",  "renamed", "raw",    "spawn",
                                      "death", "truthy",  "changed"};

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

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 7; ++index) {
            if (value.string.size == strlen(strings[index]) &&
                !memcmp(value.string.data, strings[index], value.string.size)) {
                output[1] = index + 1u;
                break;
            }
        }
    }
}

static void snapshot(Fixture *fixture, uint32_t output[20]) {
    for (size_t field = 0; field < 4; ++field) {
        value_words(fixture, fixture->object[field].value, output + field * 2u);
    }
    value_words(fixture, fixture->raw[0].value, output + 8);
    value_words(fixture, fixture->raw[1].value, output + 10);
    value_words(fixture, fixture->host.block_table, output + 12);
    value_words(fixture, fixture->blocks.value, output + 14);
    value_words(fixture, fixture->catalog.value, output + 16);
    output[18] = fixture->host.remove != NULL;
    output[19] = fixture->count;
}

static bool remove_object(void *context, AbSpan key, AbError *error) {
    Fixture *fixture = context;
    CHECK(key.size == 6 && !memcmp(key.data, "queued", 6) && !fixture->count);
    uint32_t *event = fixture->event;
    event[0] = 1;
    event[1] = 288;
    event[2] = 0;
    event[3] = 1;
    event[4] = 1;
    value_words(fixture, fixture->raw[0].value, event + 5);
    value_words(fixture, fixture->raw[1].value, event + 7);
    value_words(fixture, fixture->object[2].value, event + 9);
    value_words(fixture, fixture->object[3].value, event + 11);
    snapshot(fixture, event + 16);
    ++fixture->count;
    uint32_t mutation = fixture->input[10];
    if (mutation & 1u) {
        fixture->object[0].value = text("renamed");
    }
    if (mutation & 2u) {
        fixture->object[2].value = scalar(fixture, 2, "", 4);
        fixture->object[3].value = scalar(fixture, 1, "", 4);
    }
    if (mutation & 4u) {
        fixture->raw[0].value = text("changed");
        fixture->raw[1].value = (AbDataValue){0};
    }
    if (mutation & 8u) {
        fixture->host.block_table = (AbDataValue){0};
    }
    if (mutation & 16u) {
        fixture->host.remove = NULL;
    }
    if (fixture->mutate_entry) {
        fixture->entry.key = span("renamed");
        fixture->entry.object = (AbDataValue){0};
    }
    return fixture->fail_remove ? ab_fail(error, "Injected removeObject failure")
                                : true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_detach_case(sample, fixture->input);
    for (size_t table = 0; table < 5; ++table) {
        fixture->values[table] =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + table};
    }
    fixture->object[0] = (AbDataEntry){text("name"), text("body")};
    fixture->object[1] =
        (AbDataEntry){text("definition"), scalar(fixture, fixture->input[3], "raw", 4)};
    fixture->object[2] =
        (AbDataEntry){text("x"), scalar(fixture, fixture->input[7], "truthy", 4)};
    fixture->object[3] =
        (AbDataEntry){text("y"), scalar(fixture, fixture->input[8], "truthy", 4)};
    fixture->tables[0] = (AbDataTable){fixture->object, 4, 4, NULL};
    fixture->raw[0] =
        (AbDataEntry){text("spawn"), scalar(fixture, fixture->input[5], "spawn", 4)};
    fixture->raw[1] = (AbDataEntry){text("deathSpawn"),
                                    scalar(fixture, fixture->input[6], "death", 4)};
    fixture->tables[1] = (AbDataTable){fixture->raw, 2, 2, NULL};
    fixture->catalog =
        (AbDataEntry){text("raw"), scalar(fixture, fixture->input[4], "truthy", 1)};
    fixture->tables[2] = (AbDataTable){&fixture->catalog, 1, 1, NULL};
    fixture->blocks =
        (AbDataEntry){text("blocks"), scalar(fixture, fixture->input[2], "truthy", 2)};
    fixture->tables[3] = (AbDataTable){&fixture->blocks, 1, 1, NULL};
    fixture->entry = (AbBlockRemovalEntry){span("queued"), fixture->values[0]};
    fixture->host = (AbBlockRemovalDetachHost){
        fixture, scalar(fixture, fixture->input[1], "truthy", 3),
        fixture->input[9] ? remove_object : NULL};
}

static bool detach_edges(void) {
    Fixture fixture;
    prepare(&fixture, 1);
    fixture.fail_remove = fixture.mutate_entry = true;
    fixture.input[10] = 15;
    AbBlockRemovalDetached output = {.spawn = text("preserved")};
    CHECK(!ab_block_removal_detach(&fixture.entry, &fixture.host, &output, NULL));
    CHECK(fixture.count == 1 && fixture.object[0].value.string.size == 7 &&
          fixture.host.block_table.kind == AB_DATA_NIL);
    CHECK(output.spawn.kind == AB_DATA_STRING && output.spawn.string.size == 9);
    return true;
}

typedef struct {
    AbBlockRemovalCleanupState *state;
    AbDataEntry *object;
    size_t pigs, gifts;
    bool fail_pig;
} Cleanup;

static bool remove_pig(void *context, AbSpan key, AbError *error) {
    Cleanup *cleanup = context;
    CHECK(key.size == 6 && !memcmp(key.data, "queued", 6));
    ++cleanup->pigs;
    cleanup->object[1].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    cleanup->object[2].value = text("changed");
    cleanup->state->level_name = text("renamed");
    return cleanup->fail_pig ? ab_fail(error, "Injected removePig failure") : true;
}

static bool collect_gift(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                         AbError *error) {
    Cleanup *cleanup = context;
    (void)error;
    CHECK(level.kind == AB_DATA_STRING && level.string.size == 7);
    CHECK(x.kind == AB_DATA_STRING && x.string.size == 7 && y.kind == AB_DATA_NIL);
    ++cleanup->gifts;
    return true;
}

static bool cleanup_edges(void) {
    AbStringMap *goals = NULL, *dead = NULL;
    CHECK(ab_string_map_open(1, 1, 4096, &goals, NULL) &&
          ab_string_map_open(1, 1, 4096, &dead, NULL));
    AbDataEntry world_entry = {text("queued"), text("body")};
    AbDataEntry dynamic_entry = world_entry;
    AbDataTable world = {&world_entry, 1, 1, NULL},
                dynamic = {&dynamic_entry, 1, 1, NULL};
    AbDataEntry fields[] = {
        {text("world"), {.kind = AB_DATA_TABLE, .table = &world}},
        {text("dynamic"), {.kind = AB_DATA_TABLE, .table = &dynamic}}};
    AbDataTable objects = {fields, 2, 2, NULL};
    AbBlockRemovalState removal = {{.kind = AB_DATA_TABLE, .table = &objects}, dead};
    AbBlockRemovalCleanupState state = {&removal, goals, text("body")};
    AbDataEntry body_fields[] = {{text("levelGoal"), {.kind = AB_DATA_NUMBER}},
                                 {text("isGiftBox"), {0}},
                                 {text("x"), {0}},
                                 {text("y"), {0}}};
    AbDataTable body = {body_fields, 4, 4, NULL};
    AbBlockRemovalEntry entry = {span("queued"),
                                 {.kind = AB_DATA_TABLE, .table = &body}};
    Cleanup context = {&state, body_fields, 0, 0, false};
    AbBlockRemovalCleanupHost host = {&context, remove_pig, collect_gift};
    for (size_t pass = 0; pass < 3; ++pass) {
        CHECK(ab_string_map_set(goals, entry.key, 1, true, NULL) &&
              ab_string_map_set(dead, entry.key, 1, true, NULL));
        world_entry.value = dynamic_entry.value = text("body");
        fields[1].value = pass == 1
                              ? (AbDataValue){0}
                              : (AbDataValue){.kind = AB_DATA_TABLE, .table = &dynamic};
        context.fail_pig = pass == 2;
        bool okay = ab_block_removal_cleanup(&entry, &state, &host, NULL);
        CHECK(okay == (pass == 0) && world_entry.value.kind == AB_DATA_NIL);
        uintptr_t value = 0;
        bool present = true;
        CHECK(ab_string_map_get(goals, entry.key, &value, &present, NULL) && !present);
        CHECK(ab_string_map_get(dead, entry.key, &value, &present, NULL) &&
              present == (pass == 1));
    }
    CHECK(context.pigs == 2 && context.gifts == 1);
    ab_string_map_close(goals);
    ab_string_map_close(dead);
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Removal detach sample %u %s[%zu]: C=%08x original=%08x\n",
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
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_DETACH_V1 4096\n"));
    size_t matched = 0, failures = 0, calls = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 11, sample, "input", &matched));
        AbBlockRemovalDetached output = {0};
        uint32_t result[9] = {0};
        result[0] =
            !ab_block_removal_detach(&fixture.entry, &fixture.host, &output, NULL);
        if (!result[0]) {
            value_words(&fixture, output.spawn, result + 1);
            value_words(&fixture, output.death_spawn, result + 3);
            value_words(&fixture, output.x, result + 5);
            value_words(&fixture, output.y, result + 7);
        }
        CHECK(words(file, result, 9, sample, "result", &matched));
        uint32_t state[20];
        snapshot(&fixture, state);
        CHECK(words(file, state, 20, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        if (fixture.count) {
            CHECK(words(file, fixture.event, 36, sample, "event", &matched));
        }
        failures += result[0];
        calls += fixture.count;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal detach reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal detach phases match: %zu words, %zu failures "
           "and %zu native-removal boundary calls.\n",
           matched, failures, calls);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return detach_edges() && cleanup_edges() ? 0 : 1;
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
