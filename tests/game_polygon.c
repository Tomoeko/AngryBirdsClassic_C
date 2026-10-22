#include "ab/object_body.h"

#include "game_polygon_cases.h"
#include "game_convex_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Polygon check failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue numeric(uint32_t value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = value};
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

static bool same_words(const uint32_t *actual, const uint32_t *expected, size_t count,
                       uint32_t sample) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Polygon %" PRIu32 " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool polygon_case(FILE *file, uint32_t sample, bool circle, bool convex,
                         bool zeros, AbPhysicsWorld *world, WorldStorage *memory) {
    uint32_t input[32] = {0}, generated[32] = {0}, expected[50];
    size_t input_count = convex ? 32u : 20u;
    CHECK(read_words(file, input, input_count));
    if (zeros) {
        game_circle_zero_case(sample, generated);
    } else if (convex) {
        game_convex_case(sample, generated);
    } else {
        game_polygon_case(sample, generated);
    }
    CHECK(memcmp(input, generated, input_count * sizeof(*input)) == 0);
    size_t count = convex ? input[20] : 3u;
    CHECK(count >= 3 && count <= AB_POLYGON_VERTICES);
    AbDataEntry points[AB_POLYGON_VERTICES][2], vertex_entries[AB_POLYGON_VERTICES];
    AbDataTable point_tables[AB_POLYGON_VERTICES];
    for (size_t vertex = 0; vertex < count; ++vertex) {
        size_t position = vertex < 3 ? 6 + vertex * 2 : 21 + (vertex - 3) * 2;
        points[vertex][0] = (AbDataEntry){text("x"), numeric(input[position])};
        points[vertex][1] = (AbDataEntry){text("y"), numeric(input[position + 1])};
        point_tables[vertex] = (AbDataTable){points[vertex], 2, 2, NULL};
        vertex_entries[vertex] = (AbDataEntry){numeric(bits((float)(vertex + 1))),
                                               table_value(point_tables + vertex)};
    }
    AbDataTable vertices = {vertex_entries, count, count, NULL};
    AbDataEntry fields[] = {
        {text("type"), text(circle ? "circle" : "polygon")},
        {text("density"), numeric(input[3])},
        {text("friction"), numeric(input[4])},
        {text("restitution"), numeric(input[5])},
        {text("vertices"), table_value(&vertices)},
        {text("width"), input[17] & 1u ? numeric(input[1]) : (AbDataValue){0}},
        {text("height"), input[17] & 1u ? numeric(input[2]) : (AbDataValue){0}},
        {text("sizeFactorX"), input[17] & 2u ? numeric(input[15]) : (AbDataValue){0}},
        {text("sizeFactorY"), input[17] & 4u ? numeric(input[16]) : (AbDataValue){0}},
        {text("radius"),
         circle && (input[17] & 1u) ? numeric(input[1]) : (AbDataValue){0}},
    };
    AbDataTable source = {fields, 10, 10, NULL};
    AbObjectDefinition definition = {.source = &source};
    AbWorldStorage storage = prepare_storage(memory, 0, 0);
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbObjectBody object;
    CHECK(ab_object_body_create(world, &definition,
                                (AbVec2){number(input[18]), number(input[19])},
                                (AbVec2){number(input[12]), number(input[13])},
                                number(input[14]), 0, &object, NULL));
    const AbBodyMassState *body = &memory->bodies[object.body].motion.mass;
    const AbWorldFixture *fixture = memory->fixtures + object.fixture;
    uint32_t actual[50] = {
        (uint32_t)world->fixture_count, (uint32_t)body->type,
        bits(object.dimensions.width),  bits(object.dimensions.height),
        bits(object.dimensions.radius), bits(body->local_center.x),
        bits(body->local_center.y),     bits(body->mass),
        bits(body->inverse_mass),       bits(body->inertia),
        bits(body->inverse_inertia),    bits(fixture->density),
        bits(fixture->friction),        bits(fixture->restitution)};
    if (circle) {
        const AbCircle *shape = &fixture->shape.data.circle;
        actual[14] = bits(shape->radius);
        actual[16] = bits(shape->center.x);
        actual[17] = bits(shape->center.y);
    } else {
        const AbPolygon *shape = &fixture->shape.data.polygon;
        actual[14] = bits(shape->radius);
        actual[15] = (uint32_t)shape->count;
        actual[16] = bits(shape->centroid.x);
        actual[17] = bits(shape->centroid.y);
        size_t normal_offset = convex ? 34u : 18 + count * 2;
        for (size_t vertex = 0; vertex < shape->count; ++vertex) {
            actual[18 + vertex * 2] = bits(shape->vertices[vertex].x);
            actual[19 + vertex * 2] = bits(shape->vertices[vertex].y);
            actual[normal_offset + vertex * 2] = bits(shape->normals[vertex].x);
            actual[normal_offset + 1 + vertex * 2] = bits(shape->normals[vertex].y);
        }
    }
    size_t geometry_words = convex ? 35u : 15u;
    CHECK(read_words(file, expected, 15) &&
          read_words(file, expected + 15, geometry_words) &&
          same_words(actual, expected, 15 + geometry_words, sample));
    AbObjectPhysics original = {.density = 77};
    if (circle) {
        fields[9].value = numeric(bits(NAN));
        CHECK(
            !ab_object_physics(&definition, (AbVec2){80, 60}, 0.05f, &original, NULL) &&
            original.density == 77);
        return true;
    }
    vertices.count = 2;
    CHECK(!ab_object_physics(&definition, (AbVec2){80, 60}, 0.05f, &original, NULL) &&
          original.density == 77);
    vertices.count = count;
    points[0][0].value = numeric(bits(NAN));
    CHECK(!ab_object_physics(&definition, (AbVec2){80, 60}, 0.05f, &original, NULL) &&
          original.density == 77);
    return true;
}

static bool native_checks(FILE *file, bool circle, bool convex, bool zeros) {
    char header[100];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, zeros    ? "AB_ANDROID_GAME_CIRCLE_ZEROS_V1 4096\n"
                         : circle ? "AB_ANDROID_GAME_CIRCLE_V1 4096\n"
                         : convex ? "AB_ANDROID_GAME_CONVEX_V1 4096\n"
                                  : "AB_ANDROID_GAME_POLYGON_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        bool okay = polygon_case(file, sample, circle, convex, zeros, &world, &memory);
        ab_world_close(&world);
        if (!okay) {
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, zeros    ? "AB game circle zeros reference complete\n"
                         : circle ? "AB game circle reference complete\n"
                         : convex ? "AB game convex reference complete\n"
                                  : "AB game polygon reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 original %s fixtures, mass/centers, materials and "
           "metadata: %zu output words\n",
           circle   ? "circle"
           : convex ? "convex polygon"
                    : "triangle",
           convex ? (size_t)204800 : (size_t)122880);
    return true;
}

int main(int argc, char **argv) {
    bool zeros = argc == 3 && strcmp(argv[1], "--circle-zeros") == 0;
    bool circle = zeros || (argc == 3 && strcmp(argv[1], "--circle") == 0);
    bool convex = argc == 3 && strcmp(argv[1], "--convex") == 0;
    if (argc != 2 && !circle && !convex) {
        fputs("Usage: ab_game_polygon_tests [--circle|--circle-zeros|--convex] "
              "NATIVE_CORPUS\n",
              stderr);
        return 2;
    }
    FILE *file = fopen(argv[circle || convex ? 2 : 1], "rb");
    bool okay = file && native_checks(file, circle, convex, zeros);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
