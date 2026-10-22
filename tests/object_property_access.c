#include "ab/object_property_access.h"
#include "object_property_access_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Property access failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbObjectPropertyAccess object;
    AbObjectPropertyAccessHost host;
    AbDataTable tables[7];
    AbDataEntry entries[7][3];
    uint32_t input[15], events[8][7], calls;
} Fixture;

static const uint32_t table_ids[] = {201, 202, 203, 204, 301, 302, 101};
static const char *const strings[] = {"value", "changed", "first", "second",
                                      "wood",  "bad",     "new"};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue table(Fixture *fixture, size_t index) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + index};
}

static AbSpan property(const Fixture *fixture) {
    return span(fixture->input[13] ? "target" : "newTarget");
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind) {
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN};
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[11]};
        case 4:
            return text("value");
        default:
            return table(fixture, 6);
    }
}

static AbDataValue record_value(Fixture *fixture, uint32_t kind, size_t index) {
    if (kind == 2) {
        return table(fixture, index);
    }
    return kind == 3   ? (AbDataValue){.kind = AB_DATA_NUMBER}
           : kind == 4 ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}
           : kind == 5 ? text("bad")
                       : scalar(fixture, kind);
}

static void value_words(const Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 7; ++index) {
            if (value.string.size == strlen(strings[index]) && value.string.data &&
                !memcmp(value.string.data, strings[index], value.string.size)) {
                output[1] = index + 1u;
                break;
            }
        }
    } else if (value.kind == AB_DATA_TABLE) {
        for (size_t index = 0; index < 7; ++index) {
            if (value.table == fixture->tables + index) {
                output[1] = table_ids[index];
                break;
            }
        }
    }
}

static bool definition(void *context, AbDataValue name, AbDataValue *output,
                       AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(fixture->calls < 8);
    uint32_t *event = fixture->events[fixture->calls];
    event[0] = ++fixture->calls;
    value_words(fixture, name, event + 1);
    value_words(fixture, fixture->object.physics_object, event + 3);
    value_words(fixture, fixture->host.materials, event + 5);
    uint32_t phase = (fixture->calls - 1u) % 3u;
    if (phase == 0) {
        if (fixture->input[10] & 1u) {
            AbObjectPropertyAccess physical = {.physics_object = table(fixture, 0)};
            CHECK(ab_object_property_set(&physical, property(fixture),
                                         scalar(fixture, 1), error));
        }
        if (fixture->input[10] & 2u) {
            fixture->object.physics_object = scalar(fixture, 0);
        }
    } else if (phase == 1 && (fixture->input[10] & 4u)) {
        fixture->object.name = text("second");
    } else if (phase == 2) {
        if (fixture->input[10] & 8u) {
            fixture->host.materials = table(fixture, 5);
        }
        if (fixture->input[10] & 16u) {
            fixture->entries[2][0].value = text("changed");
        }
    }
    *output = record_value(fixture, fixture->input[4 + phase], 1);
    return true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    object_property_access_case(sample, fixture->input);
    for (size_t index = 0; index < 7; ++index) {
        fixture->tables[index] = (AbDataTable){fixture->entries[index], 1, 3, NULL};
        fixture->entries[index][0].key =
            (AbDataValue){.kind = AB_DATA_STRING, .string = property(fixture)};
    }
    fixture->entries[0][0].value = scalar(fixture, fixture->input[3]);
    if (!fixture->input[13]) {
        fixture->entries[0][0].key = text("target");
    }
    fixture->entries[1][0].value = scalar(fixture, fixture->input[7]);
    fixture->entries[1][1] = (AbDataEntry){text("materialName"), text("wood")};
    fixture->tables[1].count = 2;
    fixture->entries[2][0].value = scalar(fixture, fixture->input[9]);
    fixture->entries[3][0].value = text("new");
    fixture->entries[4][0] =
        (AbDataEntry){text("wood"), record_value(fixture, fixture->input[8], 2)};
    fixture->entries[5][0] = (AbDataEntry){text("wood"), table(fixture, 3)};
    fixture->tables[6].count = 0;
    fixture->object = (AbObjectPropertyAccess){
        text("first"), record_value(fixture, fixture->input[2], 0)};
    fixture->host = (AbObjectPropertyAccessHost){
        fixture, definition,
        fixture->input[10] & 32u ? scalar(fixture, 0) : table(fixture, 4)};
}

static AbDataValue lookup(const AbDataTable *table, AbSpan name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    return value ? *value : (AbDataValue){0};
}

static void snapshot(const Fixture *fixture, uint32_t output[15]) {
    value_words(fixture, fixture->object.name, output);
    value_words(fixture, fixture->object.physics_object, output + 2);
    for (size_t index = 0; index < 4; ++index) {
        value_words(fixture, lookup(fixture->tables + index, property(fixture)),
                    output + 4 + index * 2);
    }
    value_words(fixture, fixture->host.materials, output + 12);
    output[14] = fixture->calls;
}

static void get(Fixture *fixture, uint32_t output[4]) {
    AbDataValue value = {0};
    AbError error = {0};
    bool okay =
        ab_object_property_get(fixture->input[1] ? &fixture->object : NULL,
                               property(fixture), &fixture->host, &value, &error);
    output[0] = 1;
    output[1] = !okay;
    value_words(fixture, value, output + 2);
}

static void set(Fixture *fixture, uint32_t output[4]) {
    AbError error = {0};
    output[0] = 1;
    output[1] = !ab_object_property_set(fixture->input[1] ? &fixture->object : NULL,
                                        property(fixture),
                                        scalar(fixture, fixture->input[12]), &error);
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 1);
    fixture.object.physics_object = scalar(&fixture, 1);
    AbDataValue value = text("changed");
    CHECK(ab_object_property_get(&fixture.object, property(&fixture), NULL, &value,
                                 NULL) &&
          value.kind == AB_DATA_NIL && !fixture.calls);
    prepare(&fixture, 1);
    fixture.input[10] = 0;
    fixture.input[4] = 2;
    fixture.object.physics_object = scalar(&fixture, 2);
    value = text("changed");
    CHECK(!ab_object_property_get(&fixture.object, property(&fixture), &fixture.host,
                                  &value, NULL) &&
          fixture.calls == 1 && value.kind == AB_DATA_STRING);
    prepare(&fixture, 1);
    fixture.tables[0].capacity = fixture.tables[0].count;
    AbDataEntry before = fixture.entries[0][0];
    CHECK(!ab_object_property_set(&fixture.object, span("extra"), scalar(&fixture, 0),
                                  NULL) &&
          fixture.tables[0].count == 1 &&
          before.value.kind == fixture.entries[0][0].value.kind &&
          before.value.bits == fixture.entries[0][0].value.bits);
    fixture.tables[0].capacity = 3;
    CHECK(ab_object_property_set(&fixture.object, span("extra"), scalar(&fixture, 0),
                                 NULL) &&
          fixture.tables[0].count == 2 &&
          ab_data_get(fixture.tables,
                      (AbDataValue){.kind = AB_DATA_STRING, .string = span("extra")})
                  ->kind == AB_DATA_NIL);
    puts("Property access early gates, output preservation and bounded nil writes "
         "pass.");
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *phase, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Property access case %u %s word %zu: %08" PRIx32
                    " expected %08" PRIx32 "\n",
                    sample, phase, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_OBJECT_PROPERTY_ACCESS_V1 4096\n"));
    size_t matched = 0, queries = 0, gets = 0, sets = 0, failures = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 15, sample, "input", &matched));
        uint32_t results[3][4] = {{0}};
        uint32_t operation = fixture.input[14];
        if (operation == 0 || operation == 3) {
            get(&fixture, results[0]);
        }
        if (operation != 0) {
            set(&fixture, results[1]);
        }
        if (operation >= 2) {
            get(&fixture, results[2]);
        }
        for (size_t index = 0; index < 3; ++index) {
            CHECK(words(file, results[index], 4, sample, "result", &matched));
            failures += results[index][1];
        }
        gets += results[0][0] + results[2][0];
        sets += results[1][0];
        uint32_t output[15];
        snapshot(&fixture, output);
        CHECK(words(file, output, 15, sample, "state", &matched));
        for (uint32_t event = 0; event < fixture.calls; ++event) {
            CHECK(words(file, fixture.events[event], 7, sample, "query", &matched));
        }
        queries += fixture.calls;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB object property access reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original property cases match: %zu words, %zu gets, %zu sets, "
           "%zu definition queries, %zu indexing failures.\n",
           matched, gets, sets, queries, failures);
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
