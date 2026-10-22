#include "ab/level_properties.h"
#include "level_properties_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Level property check failed at %d: %s\n", __LINE__,       \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[12], diagnostics;
    AbLevelPropertyObject object;
    AbDataEntry world_entry, definition_entries[2], material_entry;
    AbDataEntry block_entry, materials_entry, catalog_entries[2];
    AbDataTable definition, material, blocks, materials, catalog, payload;
    AbLevelProperties properties;
} PropertyFixture;

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbSpan fixture_name(uint32_t index) {
    static const char *const names[] = {"Bird_1", "stone_1", "\xc3\xa9\xe9\xb3\xa5",
                                        "A"};
    if (index == 3)
        return (AbSpan){(const uint8_t *)"A\0B", 3};
    return span(names[index]);
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue scalar(PropertyFixture *fixture, uint32_t kind) {
    if (!kind)
        return (AbDataValue){0};
    if (kind < 3)
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    if (kind == 3)
        return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[8]};
    if (kind == 4)
        return text("value");
    return table(&fixture->payload);
}

static AbDataValue record(PropertyFixture *fixture, uint32_t kind, AbDataTable *value) {
    if (kind == 2)
        return table(value);
    if (kind == 3)
        return (AbDataValue){.kind = AB_DATA_NUMBER};
    return scalar(fixture, kind == 4 ? 2u : kind == 5 ? 4u : kind);
}

static bool diagnostic(void *context, AbSpan name, AbError *error) {
    (void)error;
    PropertyFixture *fixture = context;
    AbSpan expected =
        fixture->input[6] == 5 ? span("absent") : fixture_name(fixture->input[11]);
    CHECK(name.size == expected.size && !memcmp(name.data, expected.data, name.size));
    ++fixture->diagnostics;
    return true;
}

static bool prepare(PropertyFixture *fixture, uint32_t index) {
    *fixture = (PropertyFixture){0};
    level_property_case(index, fixture->input);
    fixture->definition_entries[0] =
        (AbDataEntry){text("radius"), scalar(fixture, fixture->input[3])};
    fixture->definition_entries[1] = (AbDataEntry){text("materialName"), text("wood")};
    fixture->definition = (AbDataTable){fixture->definition_entries, 2, 2, NULL};
    fixture->material_entry =
        (AbDataEntry){text("radius"), scalar(fixture, fixture->input[5])};
    fixture->material = (AbDataTable){&fixture->material_entry, 1, 1, NULL};
    fixture->block_entry = (AbDataEntry){
        text("Bird"), record(fixture, fixture->input[2], &fixture->definition)};
    fixture->blocks = (AbDataTable){&fixture->block_entry, 1, 1, NULL};
    fixture->materials_entry = (AbDataEntry){
        text("wood"), record(fixture, fixture->input[4], &fixture->material)};
    fixture->materials = (AbDataTable){&fixture->materials_entry, 1, 1, NULL};
    fixture->catalog_entries[0] =
        (AbDataEntry){text("blocks"), table(&fixture->blocks)};
    fixture->catalog_entries[1] =
        (AbDataEntry){text("materials"), table(&fixture->materials)};
    fixture->catalog = (AbDataTable){fixture->catalog_entries, 2, 2, NULL};
    AbLevelObject source = {.name = fixture_name(fixture->input[11]),
                            .definition_name = span("Bird")};
    memcpy(&source.physics.dimensions.width, fixture->input + 9, 4);
    memcpy(&source.physics.dimensions.height, fixture->input + 10, 4);
    memcpy(&source.physics.dimensions.radius, fixture->input + 8, 4);
    AbLevelBodies bodies = {&source, 1, 1};
    fixture->properties = (AbLevelProperties){.objects = &fixture->object,
                                              .world_entries = &fixture->world_entry,
                                              .capacity = 1};
    CHECK(ab_level_properties_initialize(&fixture->properties, &bodies,
                                         &fixture->catalog, NULL));
    fixture->properties.lookup.context = fixture;
    fixture->properties.lookup.missing = diagnostic;
    CHECK(ab_level_property_set(&fixture->properties, 0, span("radius"),
                                scalar(fixture, fixture->input[1]), NULL));
    return true;
}

static bool encode(PropertyFixture *fixture, AbDataValue value, uint32_t output[2]) {
    output[0] = output[1] = 0;
    if (value.kind == AB_DATA_NIL)
        return true;
    if (value.kind == AB_DATA_BOOLEAN) {
        output[0] = 1;
        output[1] = value.bits != 0;
    } else if (value.kind == AB_DATA_NUMBER) {
        output[0] = 3;
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        CHECK(value.string.size == 5 && !memcmp(value.string.data, "value", 5));
        output[0] = 4;
        output[1] = 1;
    } else {
        CHECK(value.kind == AB_DATA_TABLE && value.table == &fixture->payload);
        output[0] = 5;
        output[1] = 1;
    }
    return true;
}

static bool get(PropertyFixture *fixture, uint32_t output[3]) {
    AbDataValue result = text("unchanged"), saved = result;
    bool okay =
        ab_level_property_get(&fixture->properties, 0, span("radius"), &result, NULL);
    output[0] = !okay;
    if (okay)
        CHECK(encode(fixture, result, output + 1));
    else
        CHECK(!memcmp(&saved, &result, sizeof(saved)));
    return true;
}

static bool run(PropertyFixture *fixture, uint32_t output[18]) {
    memset(output, 0, LEVEL_PROPERTY_OUTPUT_WORDS * sizeof(*output));
    CHECK(get(fixture, output));
    switch (fixture->input[6]) {
        case 1:
            CHECK(ab_level_property_set(&fixture->properties, 0, span("radius"),
                                        scalar(fixture, fixture->input[7]), NULL));
            break;
        case 2:
            CHECK(ab_level_property_set(&fixture->properties, 0, span("name"),
                                        text("value"), NULL));
            break;
        case 3:
            CHECK(ab_level_properties_forget(&fixture->properties, 0, NULL));
            break;
        case 4:
            fixture->object.script_entries[1].value = (AbDataValue){0};
            break;
        case 5:
            fixture->object.script_entries[0].value = text("absent");
            break;
        default:
            break;
    }
    CHECK(get(fixture, output + 3));
    output[6] = fixture->diagnostics;
    output[7] = fixture->world_entry.value.kind == AB_DATA_TABLE;
    output[8] = fixture->object.script_entries[1].value.kind == AB_DATA_TABLE;
    output[9] = fixture->input[6] == 5;
    output[10] = fixture->input[6] == 2;
    output[11] = fixture->object.physical_entries[2].value.bits;
    output[12] = fixture->object.physical_entries[3].value.bits;
    CHECK(encode(fixture, fixture->object.physical_entries[4].value, output + 13));
    output[15] =
        fixture->object.physical_entries[5].value.table == &fixture->object.script;
    output[16] =
        fixture->object.script_entries[2].value.table == &fixture->object.behaviors;
    output[17] = fixture->object.behaviors.count == 0;
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[index]) {
            fprintf(stderr,
                    "Level property %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_LEVEL_PROPERTIES_V1 2048\n"));
    size_t failures = 0, diagnostics = 0;
    for (uint32_t index = 0; index < LEVEL_PROPERTY_CASES; ++index) {
        PropertyFixture fixture;
        uint32_t output[18];
        CHECK(prepare(&fixture, index) && words(file, fixture.input, 12, index));
        CHECK(run(&fixture, output) && words(file, output, 18, index));
        failures += output[0] + output[3];
        diagnostics += output[6];
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 &&
          !strcmp(marker, "AB level properties reference complete") &&
          fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched 61440 level-property words, %zu failures, %zu diagnostics\n",
           failures, diagnostics);
    return true;
}

static bool controls(void) {
    PropertyFixture fixture;
    CHECK(prepare(&fixture, 0));
    CHECK(!ab_level_properties_forget(NULL, 0, NULL));
    CHECK(!ab_level_properties_forget(&fixture.properties, 1, NULL));
    CHECK(!ab_level_property_set(&fixture.properties, 1, span("radius"), text("value"),
                                 NULL));
    char spare_keys[AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY][24];
    size_t spare_count =
        fixture.object.physical.capacity - fixture.object.physical.count;
    for (size_t index = 0; index < spare_count; ++index) {
        CHECK(snprintf(spare_keys[index], sizeof(spare_keys[index]), "spare_%zu",
                       index) > 0);
        CHECK(ab_level_property_set(&fixture.properties, 0, span(spare_keys[index]),
                                    text("value"), NULL));
    }
    CHECK(!ab_level_property_set(&fixture.properties, 0, span("overflow"),
                                 text("value"), NULL));
    CHECK(fixture.object.physical.count == AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY);
    CHECK(!ab_level_properties_publish_launch(&fixture.properties, 0, (AbVec2){4, 0},
                                              NULL) &&
          !fixture.object.launch_published);
    CHECK(prepare(&fixture, 0));
    fixture.object.script_entries[1].value = (AbDataValue){0};
    CHECK(ab_level_properties_publish_position(&fixture.properties, 0,
                                               (AbVec2){-0.0f, 7}, NULL));
    CHECK(fixture.object.physical_entries[6].value.bits == UINT32_C(0x80000000) &&
          fixture.object.physical_entries[7].value.bits == UINT32_C(0x40e00000));
    AbObjectProperties motion = {.pose = {{2, 3}, -0.0f},
                                 .velocity = {-0.0f, 7},
                                 .sleeping = true,
                                 .outside_boundaries = false};
    CHECK(ab_level_properties_publish_motion(&fixture.properties, 0, &motion, NULL));
    const AbDataValue *velocity = ab_data_get(&fixture.object.physical, text("xVel"));
    const AbDataValue *angle = ab_data_get(&fixture.object.physical, text("angle"));
    const AbDataValue *sleeping =
        ab_data_get(&fixture.object.physical, text("sleeping"));
    CHECK(velocity && velocity->kind == AB_DATA_NUMBER &&
          velocity->bits == UINT32_C(0x80000000) && angle &&
          angle->bits == UINT32_C(0x80000000) && sleeping &&
          sleeping->kind == AB_DATA_BOOLEAN && sleeping->bits == 1);
    CHECK(prepare(&fixture, 0));
    fixture.object.script_entries[1].value = (AbDataValue){0};
    CHECK(ab_level_properties_publish_launch(&fixture.properties, 0, (AbVec2){-0.0f, 1},
                                             NULL) &&
          fixture.object.launch_published &&
          fixture.object.physical_entries[6].value.table ==
              &fixture.object.launch_velocity &&
          fixture.object.launch_entries[0].value.bits == UINT32_C(0x80000000) &&
          fixture.object.launch_entries[1].value.bits == UINT32_C(0x3f800000));
    CHECK(!ab_level_properties_publish_launch(&fixture.properties, 0, (AbVec2){9, 10},
                                              NULL) &&
          fixture.object.launch_entries[0].value.bits == UINT32_C(0x80000000));
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
