#include "ab/classic_joints.h"

#include "game_joint_break_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Joint breakage check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t names[7], count;
    uint32_t fail_after;
} BreakRequests;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static bool request_break(void *context, AbSpan name, AbError *error) {
    BreakRequests *requests = context;
    if (requests->count == requests->fail_after) {
        return ab_fail(error, "Deferred destruction queue full");
    }
    CHECK(requests->count < 7);
    for (uint32_t index = 0; index < 7; ++index) {
        if (name.size == strlen(joint_break_names[index]) &&
            (!name.size ||
             memcmp(name.data, joint_break_names[index], name.size) == 0)) {
            requests->names[requests->count++] = index;
            return true;
        }
    }
    return false;
}

static BreakRequests empty_requests(void) {
    BreakRequests result = {.fail_after = UINT32_MAX};
    for (size_t index = 0; index < 7; ++index) {
        result.names[index] = UINT32_MAX;
    }
    return result;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    return fgetc(file) == '\n';
}

static bool check_input(FILE *file, const uint32_t *words, size_t count) {
    uint32_t expected[7];
    CHECK(count <= 7 && read_words(file, expected, count));
    return memcmp(expected, words, count * sizeof(*words)) == 0;
}

static bool break_case(FILE *file, uint32_t sample, AbPhysicsWorld *world) {
    uint32_t header[5], rows[7][7];
    game_joint_break_case(sample, header, rows);
    CHECK(check_input(file, header, 5));
    for (size_t index = 0; index < 7; ++index) {
        CHECK(check_input(file, rows[index], 7));
    }
    AbClassicJointRecord records[6], glue[1];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 6, glue, 1}, NULL));
    world->locked = true;
    for (size_t index = 0; index < 7; ++index) {
        if (index >= header[3] && (index != 6 || !header[4])) {
            continue;
        }
        const uint32_t *row = rows[index];
        AbClassicJoint definition = {
            .type = index == 6 ? AB_CLASSIC_JOINT_GLUE
                               : (AbClassicJointType)(1 + index % 4),
            .name = text(joint_break_names[row[0]]),
            .endpoints = {text(joint_break_objects[row[1]]),
                          text(joint_break_objects[row[2]])},
            .breakable = {.kind = row[3] == 1 ? AB_DATA_BOOLEAN : AB_DATA_NIL,
                          .bits = row[3] == 1 ? row[4] : 0},
            .break_force = {.kind = row[5] == 3 ? AB_DATA_NUMBER : AB_DATA_NIL,
                            .bits = row[5] == 3 ? row[6] : 0}};
        definition.joint.constraint.kind = (AbJointKind)(index % 4);
        size_t slot;
        CHECK(ab_classic_joints_create(&joints, &definition, &slot, NULL) &&
              slot == SIZE_MAX);
    }
    world->locked = (sample & 64u) != 0;
    AbPhysicsWorld before = *world;
    AbClassicJointRecord before_records[6], before_glue[1];
    memcpy(before_records, records, header[3] * sizeof(*records));
    memcpy(before_glue, glue, header[4] * sizeof(*glue));
    BreakRequests requests = empty_requests();
    CHECK(ab_classic_joints_break(&joints, text(joint_break_objects[header[1]]),
                                  number(header[2]), request_break, &requests, NULL));
    uint32_t count, expected[7];
    CHECK(read_words(file, &count, 1) && read_words(file, expected, 7));
    if (requests.count != count || memcmp(expected, requests.names, sizeof(expected))) {
        fprintf(stderr, "Native breakage mismatch in sample %" PRIu32 "\n", sample);
        return false;
    }
    CHECK(!joints.failed && joints.joint_count == header[3] &&
          joints.glue_count == header[4] &&
          memcmp(&before, world, sizeof(before)) == 0 &&
          memcmp(before_records, records, header[3] * sizeof(*records)) == 0 &&
          memcmp(before_glue, glue, header[4] * sizeof(*glue)) == 0);
    world->locked = false;
    return true;
}

static bool native_checks(FILE *file, AbPhysicsWorld *world) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_JOINT_BREAK_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        CHECK(break_case(file, sample, world));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game joint break reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 4,096 original deferred joint breakage dispatches, including "
         "strict thresholds, endpoint matching, duplicate names, deferred metadata "
         "and unchanged engine/registry state");
    return true;
}

static bool edge_checks(AbPhysicsWorld *world) {
    AbClassicJointRecord records[2] = {0};
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 2, NULL, 0}, NULL));
    for (size_t index = 0; index < 2; ++index) {
        records[index] = (AbClassicJointRecord){
            .definition = {.name = text(joint_break_names[index]),
                           .endpoints = {text("first"), text("first")},
                           .breakable = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
            .metadata_visible = true,
            .world_joint = SIZE_MAX};
    }
    joints.joint_count = 2;
    BreakRequests requests = empty_requests();
    CHECK(!ab_classic_joints_break(&joints, text("first"), NAN, request_break,
                                   &requests, NULL) &&
          !ab_classic_joints_break(&joints, text("first"), 1, NULL, &requests, NULL) &&
          !joints.failed && requests.count == 0);
    CHECK(ab_classic_joints_break(&joints, text("first"), 0, request_break, &requests,
                                  NULL) &&
          requests.count == 0);
    CHECK(ab_classic_joints_break(&joints, text("absent"), 1, request_break, &requests,
                                  NULL) &&
          requests.count == 0);
    requests.fail_after = 1;
    CHECK(!ab_classic_joints_break(&joints, text("first"), 1, request_break, &requests,
                                   NULL) &&
          joints.failed && requests.count == 1 && requests.names[0] == 0 &&
          joints.joint_count == 2 && records[0].metadata_visible &&
          records[1].metadata_visible);
    CHECK(!ab_classic_joints_break(&joints, text("first"), 1, request_break, &requests,
                                   NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    FILE *file = argc == 2 ? fopen(argv[1], "r") : NULL;
    if (argc == 2 && !file) {
        return 1;
    }
    WorldStorage memory;
    AbPhysicsWorld world = {0};
    bool okay = ab_world_open(&world, default_definition(),
                              prepare_storage(&memory, 0, 0), NULL) &&
                (file ? native_checks(file, &world) : edge_checks(&world));
    ab_world_close(&world);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
