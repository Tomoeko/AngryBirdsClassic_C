#include "ab/level_bodies.h"

#include "object_missing_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Missing-object check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue table_value(AbDataTable *table) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    return fgetc(file) == '\n';
}

typedef struct {
    AbSpan definition;
    uint32_t calls;
} MissingNotice;

static void missing_notice(void *context, AbSpan definition) {
    MissingNotice *notice = context;
    notice->definition = definition;
    ++notice->calls;
}

static bool missing_case(FILE *file, uint32_t sample, AbPhysicsWorld *world,
                         WorldStorage *memory) {
    uint32_t input[3];
    object_missing_case(sample, input);
    if (file) {
        uint32_t expected[3];
        CHECK(read_words(file, expected, 3) &&
              memcmp(expected, input, sizeof(input)) == 0);
    }
    AbDataTable empty = {0};
    AbDataEntry decoy = {text("known_definition"), table_value(&empty)};
    AbDataTable blocks = {&decoy, 1, 1, NULL};
    AbDataEntry catalog_entries[] = {{text("blocks"), table_value(&blocks)},
                                     {text("materials"), table_value(&empty)},
                                     {text("collider_types"), table_value(&empty)}};
    AbDataTable catalog = {catalog_entries, 3, 3, NULL};
    const char *definition = missing_definition_names[input[1]];
    AbDataEntry fields[] = {
        {text("name"), text("instance")},
        {text("definition"), text(definition)},
        {text("x"), number(input[2] & 4u ? NAN : 1)},
        {text("y"), number(input[2] & 4u ? NAN : 1)},
        {text("angle"), input[2] ? text("unused") : number(0)},
    };
    AbDataTable placement = {fields, 5, 5, NULL};
    AbDataEntry instance = {text("instance"), table_value(&placement)};
    AbDataTable placements = {&instance, 1, 1, NULL};
    AbDataEntry global = {text("world"), table_value(&placements)};
    AbDataTable globals = {&global, 1, 1, NULL};
    AbDataTableLayout layout = {
        .table = &placements, .hash_capacity = 1, .no_nil_assignments = true};
    AbLuaData data = {.globals = &globals, .layouts = &layout};
    MissingNotice notice = {0};
    AbLevelBodySource source = {.level = &data,
                                .catalog = &catalog,
                                .placement_scale = 1,
                                .physics_scale = 0.05f,
                                .derive_ground_position = true,
                                .missing_context = &notice,
                                .missing_definition = missing_notice};
    AbWorldStorage storage = prepare_storage(memory, 0, 0);
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbLevelObject objects[2];
    AbLevelBodies bodies = {.objects = objects, .capacity = 2, .count = 0};
    size_t index;
    AbStringOrderNode node;
    AbStringOrderWorkspace order = {&index, 1, &node, 1};
    CHECK(
        ab_level_bodies_create(world, &source, &bodies, &order, NULL) &&
        notice.calls == 1 && notice.definition.size == strlen(definition) &&
        memcmp(notice.definition.data, definition, notice.definition.size) == 0 &&
        bodies.count == 1 && objects[0].name.size == 6 &&
        memcmp(objects[0].name.data, "ground", 6) == 0 && world->body_count == 1 &&
        world->fixture_count == 1 &&
        memory->bodies[objects[0].physics.body].motion.mass.transform.position.x == 0 &&
        memory->bodies[objects[0].physics.body].motion.mass.transform.position.y == 5);
    if (file) {
        uint32_t native[5];
        CHECK(read_words(file, native, 5));
        /* The original API's empty-string result means no instance. Our typed
         * level loader omits it and preserves the ordinary ground baseline.
         * Original diagnostic wording/hash is retained in the pinned corpus;
         * the callback above receives the name for an outer logging boundary. */
        CHECK(native[0] == 0 && native[1] == 4 && native[2] == 0 &&
              native[3] == notice.calls && native[4] != 0);
    }
    return true;
}

static bool checks(FILE *file) {
    char header[100];
    if (file) {
        CHECK(fgets(header, sizeof(header), file) &&
              strcmp(header, "AB_ANDROID_OBJECT_MISSING_V1 64\n") == 0);
    }
    for (uint32_t sample = 0; sample < 64; ++sample) {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        bool okay = missing_case(file, sample, &world, &memory);
        ab_world_close(&world);
        if (!okay) {
            return false;
        }
    }
    if (file) {
        CHECK(fgets(header, sizeof(header), file) &&
              strcmp(header, "AB object missing reference complete\n") == 0 &&
              fgetc(file) == EOF && !ferror(file));
    }
    puts("Missing definitions notify once and omit the instance in 64 cases; "
         "unused geometry/overrides do not create replacement bodies");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("Usage: ab_object_missing_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    bool okay = (argc == 1 || file) && checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
