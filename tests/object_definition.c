#include "ab/object_definition.h"
#include "ab/object_physics.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue string(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits};
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool numeric_property(AbObjectDefinition *definition, const char *key,
                             float expected) {
    AbDataValue value = ab_object_property(definition, span(key));
    CHECK(value.kind == AB_DATA_NUMBER && value.bits == number(expected).bits);
    return true;
}

static bool edge_checks(void) {
    AbDataEntry material_fields[] = {
        {string("density"), number(3)},
        {string("friction"), number(4)},
        {string("restitution"), number(0)},
        {string("collider"), number(3)},
        {string("damageSound"), string("material")},
        {string("passThrough"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
    };
    AbDataTable material = {material_fields, 6, 6, NULL};
    AbDataEntry material_entry = {string("material"), table(&material)};
    AbDataTable materials = {&material_entry, 1, 1, NULL};
    AbDataEntry properties[] = {
        {string("materialName"), string("material")},
        {string("density"), {0}},
        {string("collider"), {0}},
        {string("z_order"), {0}},
        {string("friction"), number(0)},
        {string("controllable"), {.kind = AB_DATA_BOOLEAN}},
        {string("type"), string("box")},
        {string("width"), {0}},
        {string("height"), {0}},
        {string("radius"), number(1)},
    };
    AbDataTable source = {properties, 10, 10, NULL};
    AbDataEntry block = {string("object"), table(&source)};
    AbDataTable blocks = {&block, 1, 1, NULL};
    AbDataEntry collider_fields[] = {{string("portal"), number(8)},
                                     {string("bird"), number(3)},
                                     {string("pig"), number(4)}};
    AbDataTable colliders = {collider_fields, 3, 3, NULL};
    AbDataEntry sections[] = {{string("blocks"), table(&blocks)},
                              {string("materials"), table(&materials)},
                              {string("collider_types"), table(&colliders)}};
    AbDataTable catalog = {sections, 3, 3, NULL};
    AbObjectDefinition definition;
    AbError error = {0};
    CHECK(
        ab_object_definition_open(&catalog, span("object"), NULL, &definition, &error));
    CHECK(numeric_property(&definition, "density", 3));
    CHECK(numeric_property(&definition, "friction", 0));
    CHECK(numeric_property(&definition, "collider", 3));
    CHECK(numeric_property(&definition, "z_order", 4));
    CHECK(ab_object_property(&definition, span("controllable")).kind ==
          AB_DATA_BOOLEAN);
    CHECK(ab_object_property(&definition, span("controllable")).bits == 0);
    CHECK(ab_object_property(&definition, span("damageSound")).kind == AB_DATA_NIL);
    CHECK(ab_object_property(&definition, span("passThrough")).bits == 1);
    AbObjectPhysics physics;
    CHECK(ab_object_physics(&definition, (AbVec2){40, 20}, 0.05f, &physics, &error));
    CHECK(physics.shape.kind == AB_SHAPE_POLYGON &&
          physics.body_type == AB_BODY_DYNAMIC);
    CHECK(physics.shape.data.polygon.vertices[2].x == 0.92f &&
          physics.shape.data.polygon.vertices[2].y == 0.46f);
    CHECK(physics.dimensions.width == 1.84f && physics.dimensions.height == 0.92f &&
          physics.dimensions.radius == -1);
    properties[7].value = number(5);
    properties[8].value = number(6);
    CHECK(ab_object_physics(&definition, (AbVec2){NAN, NAN}, NAN, &physics, &error));
    CHECK(physics.shape.data.polygon.vertices[2].x == 2.5f &&
          physics.shape.data.polygon.vertices[2].y == 3);
    CHECK(physics.dimensions.width == 5 && physics.dimensions.height == 6);
    properties[8].value = (AbDataValue){0};
    CHECK(ab_object_physics(&definition, (AbVec2){40, 20}, 0.05f, &physics, &error));
    CHECK(physics.shape.data.polygon.vertices[2].x == 0.92f);
    AbObjectPhysics unchanged_physics = physics;
    CHECK(!ab_object_physics(&definition, (AbVec2){40, 20}, 0, &physics, &error));
    CHECK(memcmp(&physics, &unchanged_physics, sizeof(physics)) == 0);
    material_fields[0].value = number(0);
    CHECK(ab_object_physics(&definition, (AbVec2){40, 20}, 0.05f, &physics, &error));
    CHECK(physics.body_type == AB_BODY_STATIC &&
          physics.shape.data.polygon.vertices[2].x == 1 &&
          physics.shape.data.polygon.vertices[2].y == 0.5f);
    properties[6].value = string("circle");
    CHECK(ab_object_physics(&definition, (AbVec2){NAN, NAN}, NAN, &physics, &error));
    CHECK(physics.shape.kind == AB_SHAPE_CIRCLE &&
          physics.shape.data.circle.radius == 1);
    properties[6].value = string("unsupported");
    CHECK(!ab_object_physics(&definition, (AbVec2){40, 20}, 0.05f, &physics, &error));
    properties[6].value = string("box");
    material_fields[0].value = number(3);
    material_fields[3].value = number(8);
    CHECK(
        ab_object_definition_open(&catalog, span("object"), NULL, &definition, &error));
    CHECK(numeric_property(&definition, "z_order", 6));
    properties[2].value = number(0);
    material_fields[0].value = number(0);
    CHECK(
        ab_object_definition_open(&catalog, span("object"), NULL, &definition, &error));
    CHECK(numeric_property(&definition, "z_order", 0));
    AbDataEntry overrides[] = {{string("density"), number(2)},
                               {string("z_order"), number(0)}};
    AbDataTable instance = {overrides, 2, 2, NULL};
    CHECK(ab_object_definition_open(&catalog, span("object"), &instance, &definition,
                                    &error));
    CHECK(numeric_property(&definition, "density", 2));
    CHECK(numeric_property(&definition, "z_order", 0));
    overrides[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    overrides[1].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    properties[3].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_object_definition_open(&catalog, span("object"), &instance, &definition,
                                    &error));
    CHECK(numeric_property(&definition, "density", 0));
    CHECK(ab_object_property(&definition, span("z_order")).kind == AB_DATA_BOOLEAN);
    AbObjectDefinition unchanged = definition;
    CHECK(!ab_object_definition_open(NULL, span("object"), NULL, &definition, &error));
    CHECK(!ab_object_definition_open(&catalog, span("absent"), NULL, &definition,
                                     &error));
    sections[1].value = (AbDataValue){0};
    CHECK(!ab_object_definition_open(&catalog, span("object"), NULL, &definition,
                                     &error));
    CHECK(memcmp(&unchanged, &definition, sizeof(definition)) == 0);
    CHECK(ab_object_property(NULL, span("density")).kind == AB_DATA_NIL);
    CHECK(ab_object_property(&definition, (AbSpan){NULL, 1}).kind == AB_DATA_NIL);
    return true;
}

static int hex_digit(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

static bool compare_native(FILE *file, const AbDataTable *catalog) {
    char header[64];
    size_t count;
    CHECK(fscanf(file, "%63s%zu", header, &count) == 2);
    CHECK(strcmp(header, "AB_CLASSIC_DEFINITIONS_V1") == 0 && count <= 1024);
    size_t field_count = 0;
    for (size_t item = 0; item < count; ++item) {
        char name[64];
        size_t fields;
        CHECK(fscanf(file, "%63s%zu", name, &fields) == 2 && fields <= 512);
        AbObjectDefinition definition;
        AbError error = {0};
        CHECK(
            ab_object_definition_open(catalog, span(name), NULL, &definition, &error));
        for (size_t index = 0; index < fields; ++index) {
            char key[64], hex[2049];
            unsigned kind;
            uint32_t bits;
            size_t size;
            CHECK(fscanf(file, "%63s%u%8" SCNx32 "%zu%2048s", key, &kind, &bits, &size,
                         hex) == 5);
            CHECK(kind <= AB_DATA_STRING && size <= 1024);
            AbDataValue actual = ab_object_property(&definition, span(key));
            bool equal = (unsigned)actual.kind == kind && actual.bits == bits;
            if (kind == AB_DATA_STRING) {
                equal = equal && actual.string.size == size;
                CHECK(strlen(hex) == size * 2 || (!size && strcmp(hex, "-") == 0));
                for (size_t byte = 0; equal && byte < size; ++byte) {
                    int high = hex_digit(hex[byte * 2]);
                    int low = hex_digit(hex[byte * 2 + 1]);
                    CHECK(high >= 0 && low >= 0);
                    equal = actual.string.data[byte] == (uint8_t)(high * 16 + low);
                }
            } else {
                CHECK(size == 0 && strcmp(hex, "-") == 0);
            }
            if (!equal) {
                fprintf(stderr, "Native definition mismatch: %s.%s\n", name, key);
                return false;
            }
            ++field_count;
        }
    }
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All %zu native properties across %zu classic definitions match.\n",
           field_count, count);
    return true;
}

static bool compare_native_fixtures(FILE *file, const AbDataTable *catalog) {
    char header[64];
    size_t count;
    CHECK(fscanf(file, "%63s%zu", header, &count) == 2);
    CHECK(strcmp(header, "AB_CLASSIC_FIXTURES_V1") == 0 && count <= 1024);
    for (size_t item = 0; item < count; ++item) {
        char name[64];
        uint32_t width, height, scale, expected[24], actual[24] = {0};
        CHECK(fscanf(file, "%63s%8" SCNx32 "%8" SCNx32 "%8" SCNx32, name, &width,
                     &height, &scale) == 4);
        for (size_t index = 0; index < 24; ++index) {
            CHECK(fscanf(file, "%8" SCNx32, expected + index) == 1);
        }
        AbObjectDefinition definition;
        AbObjectPhysics physics;
        AbError error = {0};
        CHECK(
            ab_object_definition_open(catalog, span(name), NULL, &definition, &error));
        CHECK(ab_object_physics(&definition,
                                (AbVec2){from_bits(width), from_bits(height)},
                                from_bits(scale), &physics, &error));
        actual[0] = number(physics.density).bits;
        actual[1] = number(physics.friction).bits;
        actual[2] = number(physics.restitution).bits;
        actual[3] = (uint32_t)physics.shape.kind;
        if (physics.shape.kind == AB_SHAPE_CIRCLE) {
            const AbCircle *circle = &physics.shape.data.circle;
            actual[4] = number(circle->radius).bits;
            actual[5] = 1;
            actual[6] = number(circle->center.x).bits;
            actual[7] = number(circle->center.y).bits;
        } else {
            const AbPolygon *polygon = &physics.shape.data.polygon;
            CHECK(polygon->count == 4);
            actual[4] = number(polygon->radius).bits;
            actual[5] = (uint32_t)polygon->count;
            actual[6] = number(polygon->centroid.x).bits;
            actual[7] = number(polygon->centroid.y).bits;
            for (size_t vertex = 0; vertex < 4; ++vertex) {
                actual[8 + vertex * 2] = number(polygon->vertices[vertex].x).bits;
                actual[9 + vertex * 2] = number(polygon->vertices[vertex].y).bits;
                actual[16 + vertex * 2] = number(polygon->normals[vertex].x).bits;
                actual[17 + vertex * 2] = number(polygon->normals[vertex].y).bits;
            }
        }
        for (size_t index = 0; index < 24; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr,
                        "Native fixture %zu %s field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, name, index, actual[index], expected[index]);
                return false;
            }
        }
    }
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All %zu native classic fixture geometries/materials match.\n", count);
    return true;
}

static bool native_checks(const char *bytecode_path, const char *reference_path,
                          bool fixtures) {
    AbBuffer input = {0};
    AbLuaChunk chunk = {0};
    AbLuaData data = {0};
    AbError error = {0};
    FILE *file = NULL;
    bool okay = false;
    if (!ab_file_read(bytecode_path, 8 * 1024 * 1024, &input, &error) ||
        !ab_lua_read((AbSpan){input.data, input.size}, 8 * 1024 * 1024, &chunk,
                     &error) ||
        !ab_lua_data(&chunk, 8 * 1024 * 1024, &data, &error)) {
        fprintf(stderr, "Definition data: %s\n", error.message);
        goto release_data;
    }
    file = fopen(reference_path, "rb");
    if (!file) {
        perror(reference_path);
        goto release_data;
    }
    okay = fixtures ? compare_native_fixtures(file, data.globals)
                    : compare_native(file, data.globals);
release_data:
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_lua_data_close(&data);
    ab_lua_close(&chunk);
    ab_buffer_free(&input);
    return okay;
}

#include "object_body.h"
#include "level_steps.h"
#include "classic_apk.h"

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--apk-bodies") == 0) {
        return classic_apk_checks(argv[2], argv[3], NULL, NULL, NULL, NULL, NULL) ? 0
                                                                                  : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--apk-frames") == 0) {
        return classic_apk_checks(argv[2], argv[3], argv[4], NULL, NULL, NULL, NULL)
                   ? 0
                   : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--apk-startup") == 0) {
        return classic_apk_checks(argv[2], argv[3], NULL, argv[4], NULL, NULL, NULL)
                   ? 0
                   : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--apk-sprites") == 0) {
        return classic_apk_checks(argv[2], argv[3], NULL, NULL, argv[4], NULL, NULL)
                   ? 0
                   : 1;
    }
    if ((argc == 6 || argc == 7) && strcmp(argv[1], "--apk-camera") == 0) {
        return classic_apk_checks(argv[2], argv[3], argv[4], NULL, NULL, argv[5],
                                  argc == 7 ? argv[6] : NULL)
                   ? 0
                   : 1;
    }
    AbPhysicsWorld world = {0};
    bool body_edges = body_edge_checks(&world) && level_body_edge_checks(&world) &&
                      angle_edge_checks() && body_pose_edge_checks() &&
                      game_frame_edge_checks();
    ab_world_close(&world);
    if (!body_edges) {
        return 1;
    }
    if (argc == 3 && strcmp(argv[1], "--angles") == 0) {
        return native_angle_checks(argv[2]) ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "--bodies") == 0) {
        return initial_body_checks(argv[2], argv[3], argv[4]) ? 0 : 1;
    }
    if (argc == 6 && strcmp(argv[1], "--steps") == 0) {
        return level_sequence_checks(argv[2], argv[3], argv[4], argv[5], false, NULL)
                   ? 0
                   : 1;
    }
    if (argc == 6 && strcmp(argv[1], "--frames") == 0) {
        return level_sequence_checks(argv[2], argv[3], argv[4], argv[5], true, NULL)
                   ? 0
                   : 1;
    }
    if (argc == 7 && strcmp(argv[1], "--retry") == 0) {
        return level_retry_checks(argv[2], argv[3], argv[4], argv[5], argv[6]) ? 0 : 1;
    }
    if (argc == 7 &&
        (strcmp(argv[1], "--flight") == 0 || strcmp(argv[1], "--impact") == 0)) {
        return level_sequence_checks(argv[2], argv[3], argv[4], argv[5], true, argv[6])
                   ? 0
                   : 1;
    }
    bool fixtures = argc == 4 && strcmp(argv[1], "--fixtures") == 0;
    if (argc != 1 && argc != 3 && !fixtures) {
        fprintf(
            stderr,
            "Usage: ab_definition_tests [--fixtures] BLOCKS_BYTECODE NATIVE_FIELDS\n"
            "       ab_definition_tests --apk-camera APK NATIVE_BODIES "
            "INITIAL_FRAMES CAMERA_FRAMES [NEW_PRIVATE_GPU_INPUT]\n"
            "       ab_definition_tests --bodies BLOCKS_BYTECODE LEVEL_BYTECODE "
            "NATIVE_BODIES\n"
            "       ab_definition_tests --steps BLOCKS_BYTECODE LEVEL_BYTECODE "
            "NATIVE_BODIES NATIVE_STEPS\n"
            "       ab_definition_tests --frames BLOCKS_BYTECODE LEVEL_BYTECODE "
            "NATIVE_BODIES NATIVE_FRAMES\n"
            "       ab_definition_tests --flight BLOCKS_BYTECODE LEVEL_BYTECODE "
            "NATIVE_BODIES NATIVE_INITIAL_FRAMES NATIVE_FLIGHT\n"
            "       ab_definition_tests --impact BLOCKS_BYTECODE LEVEL_BYTECODE "
            "NATIVE_BODIES NATIVE_INITIAL_FRAMES NATIVE_IMPACT\n"
            "       ab_definition_tests --retry BLOCKS_BYTECODE LEVEL_BYTECODE "
            "NATIVE_BODIES NATIVE_INITIAL_FRAMES NATIVE_RETRY\n"
            "       ab_definition_tests --angles NATIVE_ANGLE_TRACE\n"
            "       ab_definition_tests\n");
        return 2;
    }
    int first = fixtures ? 2 : 1;
    return edge_checks() &&
                   (argc == 1 || native_checks(argv[first], argv[first + 1], fixtures))
               ? 0
               : 1;
}
