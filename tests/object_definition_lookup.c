#include "ab/object_definition_lookup.h"
#include "object_definition_lookup_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Definition lookup check failed at %d: %s\n", __LINE__,    \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[DEFINITION_LOOKUP_INPUT_WORDS];
    AbDataTable tables[DEFINITION_LOOKUP_TABLES];
    AbDataEntry entries[5];
    AbObjectDefinitionLookup lookup;
    uint32_t calls, event[DEFINITION_LOOKUP_EVENT_WORDS];
} LookupFixture;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue value(LookupFixture *fixture, uint32_t kind, size_t role) {
    if (!kind)
        return (AbDataValue){0};
    if (kind <= 2)
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    if (kind == 3)
        return (AbDataValue){.kind = AB_DATA_NUMBER,
                             .bits = fixture->input[role == 6 ? 11 : 10]};
    if (kind == 4)
        return text("value");
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + role};
}

static AbDataValue lookup_identifier(LookupFixture *fixture) {
#if defined(AB_TEST_OBJECT_WORLD_LOOKUP)
    if (fixture->input[0] % 2u == 0)
        return value(fixture, fixture->input[6], 5);
#endif
    DefinitionLookupName name = definition_lookup_name(fixture->input[8]);
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name.data, name.size}};
}

static bool missing(void *context, AbSpan name, AbError *error) {
    LookupFixture *fixture = context;
    CHECK(!fixture->calls);
    ++fixture->calls;
    fixture->event[0] = fixture->calls;
    fixture->event[1] = 9;
    fixture->event[2] = 21;
    fixture->event[3] = 1;
    CHECK(name.size <= UINT32_MAX);
    definition_lookup_message(
        (DefinitionLookupName){(const char *)name.data, (uint32_t)name.size},
        fixture->event + 4);
    if (fixture->input[9] & 4u)
        fixture->lookup.objects = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    return !(fixture->input[9] & 2u) || ab_fail(error, "lookup-host-rejected");
}

static void prepare(LookupFixture *fixture) {
    for (size_t index = 0; index < DEFINITION_LOOKUP_TABLES; ++index) {
        fixture->tables[index] = (AbDataTable){0};
        if (index < 5)
            fixture->tables[index] =
                (AbDataTable){fixture->entries + index, 1, 1, NULL};
    }
    AbDataValue identifier = lookup_identifier(fixture);
#if defined(AB_TEST_OBJECT_WORLD_LOOKUP)
    if (fixture->input[0] % 2u == 0 &&
        definition_lookup_invalid_key(fixture->input[6], fixture->input[10]))
        fixture->tables[1].count = 0;
#endif
    fixture->entries[0] =
        (AbDataEntry){text("world"), value(fixture, fixture->input[2], 1)};
    fixture->entries[1] =
        (AbDataEntry){identifier, value(fixture, fixture->input[3], 2)};
    fixture->entries[2] =
        (AbDataEntry){text("definition"), value(fixture, fixture->input[6], 5)};
    fixture->entries[3] =
        (AbDataEntry){text("blocks"), value(fixture, fixture->input[5], 4)};
    AbDataValue stored = value(fixture, fixture->input[6], 5);
    if (fixture->input[12] == 1)
        stored = text("other");
    else if (fixture->input[12] == 2)
        stored.bits ^= UINT32_C(0x80000000);
    else if (fixture->input[12] == 3)
        stored = value(fixture, 5, 6);
    fixture->entries[4] = (AbDataEntry){stored, value(fixture, fixture->input[7], 6)};
    if (!fixture->input[12] &&
        definition_lookup_invalid_key(fixture->input[6], fixture->input[10]))
        fixture->tables[4].count = 0;
    fixture->lookup = (AbObjectDefinitionLookup){
        value(fixture, fixture->input[1], 0), value(fixture, fixture->input[4], 3),
        fixture, fixture->input[9] & 1u ? NULL : missing};
}

static bool encode(const LookupFixture *fixture, AbDataValue input,
                   uint32_t output[2]) {
    output[0] = output[1] = 0;
    if (input.kind == AB_DATA_NIL)
        return true;
    if (input.kind == AB_DATA_BOOLEAN) {
        output[0] = 1;
        output[1] = input.bits != 0;
    } else if (input.kind == AB_DATA_NUMBER) {
        output[0] = 3;
        output[1] = input.bits;
    } else if (input.kind == AB_DATA_STRING) {
        CHECK(input.string.size == 5 && !memcmp(input.string.data, "value", 5));
        output[0] = 4;
        output[1] = 1;
    } else {
        CHECK(input.kind == AB_DATA_TABLE);
        output[0] = 5;
        for (size_t index = 0; index < DEFINITION_LOOKUP_TABLES; ++index) {
            if (input.table == fixture->tables + index)
                output[1] = (uint32_t)index + 1u;
        }
        CHECK(output[1]);
    }
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t item) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[index]) {
            fprintf(stderr,
                    "Definition lookup %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[96];
#if defined(AB_TEST_OBJECT_WORLD_LOOKUP)
    const char *header = "AB_ANDROID_OBJECT_WORLD_LOOKUP_V1 4096\n";
    const char *footer = "AB object world lookup reference complete";
#else
    const char *header = "AB_ANDROID_OBJECT_DEFINITION_LOOKUP_V1 4096\n";
    const char *footer = "AB object definition lookup reference complete";
#endif
    CHECK(fgets(marker, sizeof(marker), file) && !strcmp(marker, header));
    size_t matched = 0, failures = 0, diagnostics = 0;
    for (uint32_t item = 0; item < DEFINITION_LOOKUP_CASES; ++item) {
        LookupFixture fixture = {0};
        object_definition_lookup_case(item, fixture.input);
        CHECK(words(file, fixture.input, DEFINITION_LOOKUP_INPUT_WORDS, item));
        prepare(&fixture);
        AbDataValue result = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x41880000)};
        AbDataValue saved = result;
#if defined(AB_TEST_OBJECT_WORLD_LOOKUP)
        bool okay = ab_object_world_lookup(&fixture.lookup, lookup_identifier(&fixture),
                                           &result, NULL);
#else
        DefinitionLookupName name = definition_lookup_name(fixture.input[8]);
        bool okay = ab_object_definition_lookup(
            &fixture.lookup, (AbSpan){(const uint8_t *)name.data, name.size}, &result,
            NULL);
#endif
        uint32_t output[DEFINITION_LOOKUP_OUTPUT_WORDS] = {0};
        output[0] = !okay;
        if (okay)
            CHECK(encode(&fixture, result, output + 1));
        else
            CHECK(!memcmp(&result, &saved, sizeof(result)));
        CHECK(encode(&fixture, fixture.lookup.objects, output + 3));
        output[5] = fixture.calls;
        CHECK(words(file, output, DEFINITION_LOOKUP_OUTPUT_WORDS, item));
        if (fixture.calls)
            CHECK(words(file, fixture.event, DEFINITION_LOOKUP_EVENT_WORDS, item));
        matched += DEFINITION_LOOKUP_INPUT_WORDS + DEFINITION_LOOKUP_OUTPUT_WORDS +
                   (size_t)fixture.calls * DEFINITION_LOOKUP_EVENT_WORDS;
        failures += !okay;
        diagnostics += fixture.calls;
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 && !strcmp(marker, footer));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched %zu definition-lookup words, %zu failures, %zu diagnostics\n",
           matched, failures, diagnostics);
    return true;
}

static bool controls(void) {
    LookupFixture fixture = {
        .input = {0, 5, 5, 5, 5, 5, 4, 3, 1, 0, 0, UINT32_C(0x42280000), 0}};
    prepare(&fixture);
    AbDataValue output = {0};
#if defined(AB_TEST_OBJECT_WORLD_LOOKUP)
    CHECK(ab_object_world_lookup(&fixture.lookup, lookup_identifier(&fixture), &output,
                                 NULL));
    CHECK(output.kind == AB_DATA_TABLE && output.table == fixture.tables + 2);
#else
    CHECK(ab_object_definition_lookup(
        &fixture.lookup, (AbSpan){(const uint8_t *)"bird", 4}, &output, NULL));
    CHECK(output.kind == AB_DATA_NUMBER && output.bits == UINT32_C(0x42280000));
#endif
    CHECK(!ab_object_definition_lookup(NULL, (AbSpan){0}, &output, NULL));
    fixture.tables[0].count = 2;
    AbDataValue saved = output;
    CHECK(!ab_object_definition_lookup(&fixture.lookup, (AbSpan){0}, &output, NULL) &&
          !memcmp(&saved, &output, sizeof(output)));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return controls() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = reference(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
