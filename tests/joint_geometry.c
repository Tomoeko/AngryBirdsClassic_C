#include "ab/physics_joint.h"

#include "joint_geometry_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Joint geometry failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool edges(void) {
    AbJointGeometryInput input = {
        .kind = AB_JOINT_DISTANCE,
        .bodies = {{{.cosine = 1.0f}, NAN}, {{.cosine = 1.0f}, NAN}},
        .world_anchors = {{2, 3}, {5, 7}},
        .world_axis = {NAN, NAN},
    };
    AbJointGeometry output;
    CHECK(ab_joint_geometry(&input, &output, NULL) && output.length == 5 &&
          output.local_anchors[0].x == 2 && output.local_anchors[0].y == 3 &&
          output.local_anchors[1].x == 5 && output.local_anchors[1].y == 7 &&
          bits(output.reference_angle) == 0 && bits(output.local_axis.x) == 0);
    input.kind = AB_JOINT_WELD;
    input.bodies[0].angle = 0.25f;
    input.bodies[1].angle = 1.25f;
    input.world_anchors[1] = (AbVec2){NAN, NAN};
    CHECK(ab_joint_geometry(&input, &output, NULL) && output.reference_angle == 1 &&
          output.local_anchors[1].x == 2 && output.local_anchors[1].y == 3 &&
          bits(output.length) == 0);
    input.kind = AB_JOINT_PRISMATIC;
    input.world_axis = (AbVec2){2, 3};
    CHECK(ab_joint_geometry(&input, &output, NULL) && output.local_axis.x == 2 &&
          output.local_axis.y == 3 && output.reference_angle == 1);
    AbJointGeometry before = output;
    CHECK(!ab_joint_geometry(NULL, &output, NULL) &&
          !ab_joint_geometry(&input, NULL, NULL));
    input.kind = (AbJointKind)-1;
    CHECK(!ab_joint_geometry(&input, &output, NULL));
    input.kind = (AbJointKind)4;
    CHECK(!ab_joint_geometry(&input, &output, NULL));
    input.kind = AB_JOINT_PRISMATIC;
    input.world_axis.x = INFINITY;
    CHECK(!ab_joint_geometry(&input, &output, NULL));
    input.world_axis.x = 2;
    input.bodies[1].transform.cosine = NAN;
    CHECK(!ab_joint_geometry(&input, &output, NULL));
    input.bodies[1].transform.cosine = 1;
    input.bodies[0].transform.position.x = -FLT_MAX;
    input.world_anchors[0].x = FLT_MAX;
    CHECK(!ab_joint_geometry(&input, &output, NULL) &&
          memcmp(&before, &output, sizeof(output)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static AbJointGeometryInput make_input(const uint32_t encoded[16], AbJointKind kind) {
    float values[16];
    for (size_t index = 0; index < 16; ++index) {
        memcpy(values + index, encoded + index, sizeof(float));
    }
    AbJointGeometryInput input = {
        .kind = kind,
        .world_anchors = {{values[10], values[11]}, {values[12], values[13]}},
        .world_axis = {values[14], values[15]}};
    for (size_t body = 0; body < 2; ++body) {
        size_t offset = body * 5;
        input.bodies[body] = (AbJointBodyFrame){{{values[offset], values[offset + 1]},
                                                 values[offset + 2],
                                                 values[offset + 3]},
                                                values[offset + 4]};
    }
    return input;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_JOINT_GEOMETRY_V1 4096 4\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t ordinal, encoded[16], generated[16];
        CHECK(words(file, &ordinal, 1) && ordinal == sample &&
              words(file, encoded, 16));
        joint_geometry_case(sample, generated);
        for (size_t index = 0; index < 16; ++index) {
            if (encoded[index] != generated[index]) {
                fprintf(stderr, "Joint input sample %" PRIu32 " field %zu differs\n",
                        sample, index);
                return false;
            }
        }
        for (size_t kind = 0; kind < 4; ++kind) {
            uint32_t expected[8];
            CHECK(words(file, expected, 8));
            AbJointGeometryInput input = make_input(encoded, (AbJointKind)kind);
            AbJointGeometry actual;
            CHECK(ab_joint_geometry(&input, &actual, NULL));
            const float values[8] = {
                actual.local_anchors[0].x, actual.local_anchors[0].y,
                actual.local_anchors[1].x, actual.local_anchors[1].y,
                actual.local_axis.x,       actual.local_axis.y,
                actual.reference_angle,    actual.length,
            };
            for (size_t index = 0; index < 8; ++index) {
                if (bits(values[index]) != expected[index]) {
                    fprintf(stderr,
                            "Joint sample %" PRIu32 " kind %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            sample, kind, index, bits(values[index]), expected[index]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB joint geometry reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Compared 16,384 native joint initializations and all defined geometry bits");
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fputs("Usage: ab_joint_geometry_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
