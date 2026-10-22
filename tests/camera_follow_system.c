#include "ab/camera_system.h"
#include "ab/camera_follow_world.h"
#include "ab/bird_camera_behavior.h"
#include "ab/level_properties.h"
#include "ab/target_data.h"
#include "camera_follow_system_cases.h"
#include "camera_state_words.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Owner follow check failed at %d: %s\n", __LINE__,         \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
#include "world_raycast_fixture.h"
#endif

typedef struct {
    uint32_t config[OWNER_FOLLOW_CONFIG_WORDS], command[OWNER_FOLLOW_COMMAND_WORDS];
    uint32_t events[OWNER_FOLLOW_MAX_EVENTS][OWNER_FOLLOW_EVENT_WORDS];
    uint32_t count, counts[12], publication[4];
    AbCameraSnap snaps[2];
    AbCameraFollowBird birds[2];
    AbCameraFollowHost environment;
    AbCameraSystem *system;
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
    WorldRayFixture world_query;
    AbLevelBodies world_names;
    AbBirdImpactWorld world_source;
    AbBirdImpactWorldHost world_services;
    AbCameraFollowWorld world_binding;
    float gravity;
    uint32_t prediction_bird, prediction_rays, prediction_radii;
#if defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
    AbLevelPropertyObject property_objects[2];
    AbDataEntry property_world[2], property_block, property_catalog_entries[2];
    AbDataTable property_definition, property_blocks, property_catalog;
    AbLevelProperties properties;
#endif
#endif
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
    const AbCameraSequence *end_template;
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    const AbCameraSequence *start_template;
    uint32_t flying;
#endif
    AbCameraSequence end_sequence;
    AbCameraSequenceStage end_stages[3];
#endif
} OwnerFixture;

static uint32_t *event(OwnerFixture *fixture, uint32_t kind, uint32_t pc,
                       uint32_t size) {
    if (fixture->count >= OWNER_FOLLOW_MAX_EVENTS || kind < 1 || kind > 11)
        return NULL;
    uint32_t *row = fixture->events[fixture->count++];
    memset(row, 0, OWNER_FOLLOW_EVENT_WORDS * sizeof(*row));
    row[0] = kind;
    row[1] = ++fixture->counts[kind];
    row[2] = pc;
    row[3] = size;
    return row;
}

#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
#include "camera_follow_end_sequence.h"
#endif
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
#include "camera_follow_world_fixture.h"
#endif

static bool environment(void *context, AbCameraFollowCall *call, AbError *error) {
    OwnerFixture *fixture = context;
    uint32_t *row;
    if (call->operation == AB_CAMERA_FOLLOW_PREDICT) {
        row = event(fixture, 2, 18, 278);
        CHECK(row);
        if (call->bird == fixture->birds)
            row[4] = 1;
        else if (call->bird == fixture->birds + 1)
            row[4] = 2;
        else
            return ab_fail(error, "Unknown owned follow prediction target");
        CHECK(call->bird->launch_present);
        row[6] = owner_follow_word(call->bird->position.x);
        row[7] = owner_follow_word(call->bird->position.y);
        row[8] = owner_follow_word(call->bird->launch_velocity.x);
        row[9] = owner_follow_word(call->bird->launch_velocity.y);
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
        return owner_world_predict(fixture, call, row, error);
#else
        row[10] = fixture->config[13];
        row[11] = fixture->config[14];
        row[12] = fixture->config[15];
        row[13] = !(fixture->config[18] & 1u);
        call->prediction = (AbBirdImpactPrediction){
            .point = {owner_follow_number(row[10]), owner_follow_number(row[11])},
            .time = owner_follow_number(row[12]),
            .point_present = row[13] != 0};
        return true;
#endif
    }
    if (call->operation == AB_CAMERA_FOLLOW_TRANSFORM) {
        uint32_t pc = call->site == AB_CAMERA_FOLLOW_INITIAL ? 36u : 130u;
        row = event(fixture, 3, pc, 278);
        CHECK(row);
        row[6] = owner_follow_word(call->point.x);
        row[7] = owner_follow_word(call->point.y);
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
        CHECK(owner_world_service(fixture, call, error));
#else
        float scale = owner_follow_number(fixture->config[17]);
        call->vector = (AbVec2){call->point.x * scale, call->point.y * scale};
#endif
        row[8] = owner_follow_word(call->vector.x);
        row[9] = owner_follow_word(call->vector.y);
        return true;
    }
    if (call->operation == AB_CAMERA_FOLLOW_SEQUENCE) {
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
        return owner_world_service(fixture, call, error);
#elif defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
        return start_end_sequence(fixture, error);
#else
        row = event(fixture, 4, 270, 278);
        CHECK(row);
        row[4] = 1;
        row[6] = 1;
        return true;
#endif
    }
    return ab_fail(error, "Unexpected external owner follow operation");
}

static bool open_owner_fixture(OwnerFixture *fixture, uint32_t scene) {
    camera_follow_system_config(scene, fixture->config);
    const uint32_t *input = fixture->config;
    for (size_t index = 0; index < 2; ++index) {
        size_t snap = index ? 6u : 3u;
        fixture->snaps[index] = (AbCameraSnap){
            {owner_follow_number(input[snap]), owner_follow_number(input[snap + 1])},
            owner_follow_number(input[snap + 2])};
        size_t bird = index ? 19u : 9u;
        fixture->birds[index] = (AbCameraFollowBird){
            .position = {owner_follow_number(input[bird]),
                         owner_follow_number(input[bird + 1])},
            .launch_velocity = {owner_follow_number(input[bird + 2]),
                                owner_follow_number(input[bird + 3])},
            .launch_present = index != 0 || !(input[18] & 2u)};
    }
    fixture->environment =
        (AbCameraFollowHost){fixture, owner_follow_number(input[16]), environment};
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    bool opened = ab_camera_system_open(
#else
    return ab_camera_system_open(
#endif
        fixture->snaps, 2,
        (AbVec2){owner_follow_number(input[1]), owner_follow_number(input[2])}, 200,
        200, 65536, &fixture->system, NULL);
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    if (!opened)
        return false;
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
    if (!owner_world_open(fixture))
        return false;
#endif
    return ab_camera_system_bind_follow(fixture->system, fixture, resolve_shot_target);
#endif
}

static bool action(OwnerFixture *fixture) {
    const uint32_t *command = fixture->command;
    uint32_t operation = command[0];
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
    if (operation == 10)
        return start_end_sequence(fixture, NULL);
#endif
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    if (operation == 1 || operation == 2) {
        fixture->flying = operation;
        return start_shot_behavior(fixture);
    }
#endif
    if (operation == 1 || operation == 2) {
        bool allow = false;
        float compensation = 0;
        return ab_camera_system_follow_bird(
            fixture->system, fixture->birds + (operation - 1u),
            fixture->config[18] & 4u ? NULL : &allow,
            fixture->config[18] & 8u ? &compensation : NULL, &fixture->environment,
            NULL);
    }
    if (operation == 3 || operation == 4 || operation == 8) {
        return ab_camera_system_remove_bird(
            fixture->system, operation == 8 ? NULL : fixture->birds + (operation - 3u));
    }
    if (operation == 5)
        return ab_camera_system_follow_bird(fixture->system, NULL, NULL, NULL, NULL,
                                            NULL);
    AbCameraMove move = {.duration = owner_follow_number(command[7]),
                         .duration_present = true};
    if (operation == 6 || operation == 9) {
        AbCameraValues values = {.position = {owner_follow_number(command[4]),
                                              owner_follow_number(command[5])},
                                 .zoom = owner_follow_number(command[6]),
                                 .present = command[3]};
        CHECK(ab_camera_system_goto_position(fixture->system, &values, &move, NULL));
        return operation != 9 || ab_camera_system_remove_bird(fixture->system, NULL);
    }
    if (operation == 7)
        return ab_camera_system_goto_snap(fixture->system, 0, true, true, &move, NULL);
    return operation == 0;
}

static bool publish(OwnerFixture *fixture) {
    AbCameraSystemView view = ab_camera_system_view(fixture->system);
    CHECK(view.viewport);
    uint32_t *row = event(fixture, 5, 51, 59);
    CHECK(row);
    row[6] = owner_follow_word(view.viewport->rendering.top_left.x);
    row[7] = owner_follow_word(view.viewport->rendering.top_left.y);
    fixture->publication[0] = row[6];
    fixture->publication[1] = row[7];
    ++fixture->publication[3];
    row = event(fixture, 6, 57, 59);
    CHECK(row);
    row[6] = owner_follow_word(view.mover->current_zoom);
    fixture->publication[2] = row[6];
    ++fixture->publication[3];
    return true;
}

static bool snapshot(const OwnerFixture *fixture,
                     uint32_t output[OWNER_FOLLOW_OUTPUT_WORDS]) {
    AbCameraSystemView view = ab_camera_system_view(fixture->system);
    const AbCameraFollowBird *target = ab_camera_system_target_bird(fixture->system);
    uint32_t identity = target == fixture->birds       ? 1u
                        : target == fixture->birds + 1 ? 2u
                                                       : 0;
    CHECK(camera_state_words(view, identity, output));
    for (uint32_t index = 0; index < 4; ++index)
        output[43 + index] = fixture->publication[index];
    output[47] = fixture->counts[4];
    output[48] = fixture->counts[2];
    output[49] = fixture->counts[3];
    output[50] = fixture->count;
    output[63] = fixture->command[11];
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
    end_sequence_snapshot(fixture, output);
#endif
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t scene,
                  uint32_t step, const char *stage) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected = 0;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Owner follow scene %u step %u %s word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    scene, step, stage, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool compare_scene(FILE *file, OwnerFixture *fixture, uint32_t scene,
                          size_t *matched, size_t *boundaries, size_t *failures) {
    CHECK(words(file, fixture->config, OWNER_FOLLOW_CONFIG_WORDS, scene, 0, "config"));
    *matched += OWNER_FOLLOW_CONFIG_WORDS;
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
    for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
        uint32_t row[WORLD_RAY_BODY_WORDS];
        world_raycast_body(fixture->config[0], index, row);
        CHECK(words(file, row, WORLD_RAY_BODY_WORDS, scene, index, "world body"));
        *matched += WORLD_RAY_BODY_WORDS;
    }
#endif
    for (uint32_t step = 0; step < OWNER_FOLLOW_STEPS; ++step) {
        camera_follow_system_command(scene, step, fixture->command);
        CHECK(words(file, fixture->command, OWNER_FOLLOW_COMMAND_WORDS, scene, step,
                    "command"));
        fixture->count = 0;
        CHECK(action(fixture));
        uint32_t *row = event(fixture, 1, 3, 59);
        CHECK(row);
        row[4] = 501;
        row[6] = fixture->command[1];
        row[10] = fixture->command[2];
        row[11] = fixture->command[3];
        const uint32_t *command = fixture->command;
        AbCameraSystemInput input = {
            .targets = {.values = {.position = {owner_follow_number(command[4]),
                                                owner_follow_number(command[5])},
                                   .zoom = owner_follow_number(command[6]),
                                   .present = command[3]},
                        .movement_duration = owner_follow_number(command[7]),
                        .zoom_duration = owner_follow_number(command[8])},
            .targets_present = (command[2] & 2u) != 0,
            .user_input = (command[2] & 1u) != 0,
            .moving_objects = (command[2] & 4u) != 0,
            .shake = {owner_follow_number(command[9]),
                      owner_follow_number(command[10])}};
        AbError error = {0};
        bool okay = ab_camera_system_update(
            fixture->system, owner_follow_number(command[1]), &input, &error);
        if (okay)
            CHECK(publish(fixture));
        uint32_t output[OWNER_FOLLOW_OUTPUT_WORDS] = {0};
        output[0] = !okay;
        CHECK(snapshot(fixture, output));
        if (!words(file, output, OWNER_FOLLOW_OUTPUT_WORDS, scene, step, "state")) {
            if (error.message[0])
                fprintf(stderr, "%s\n", error.message);
            return false;
        }
        for (size_t index = 0; index < fixture->count; ++index) {
            CHECK(words(file, fixture->events[index], OWNER_FOLLOW_EVENT_WORDS, scene,
                        step, "event"));
        }
        *matched += OWNER_FOLLOW_COMMAND_WORDS + OWNER_FOLLOW_OUTPUT_WORDS +
                    (size_t)fixture->count * OWNER_FOLLOW_EVENT_WORDS;
        *boundaries += fixture->count;
        *failures += !okay;
    }
    return true;
}

static bool reference(FILE *file, const AbCameraSequence *source) {
    (void)source;
    char marker[96];
#if defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
    const char *header = "AB_ANDROID_CAMERA_FOLLOW_PROPERTIES_V1 48 160 12\n";
    const char *footer = "AB camera follow properties reference complete";
#elif defined(AB_TEST_CAMERA_FOLLOW_WORLD)
    const char *header = "AB_ANDROID_CAMERA_FOLLOW_WORLD_V1 48 160 12\n";
    const char *footer = "AB camera follow world reference complete";
#elif defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    const char *header = "AB_ANDROID_CAMERA_SHOT_SEQUENCE_V1 16 160\n";
    const char *footer = "AB camera shot sequence reference complete";
#elif defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
    const char *header = "AB_ANDROID_CAMERA_FOLLOW_END_SEQUENCE_V1 16 160\n";
    const char *footer = "AB camera follow end sequence reference complete";
#else
    const char *header = "AB_ANDROID_CAMERA_FOLLOW_SYSTEM_V1 16 32\n";
    const char *footer = "AB camera follow system reference complete";
#endif
    CHECK(fgets(marker, sizeof(marker), file) && !strcmp(marker, header));
    size_t matched = 0, boundaries = 0, failures = 0;
    for (uint32_t scene = 0; scene < OWNER_FOLLOW_SCENES; ++scene) {
        OwnerFixture fixture = {0};
#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
        fixture.end_template = source;
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
        fixture.start_template = source + 1;
#endif
#endif
        bool okay =
            open_owner_fixture(&fixture, scene) &&
            compare_scene(file, &fixture, scene, &matched, &boundaries, &failures);
        ab_camera_system_close(fixture.system);
#if defined(AB_TEST_CAMERA_FOLLOW_WORLD)
        ab_world_close(&fixture.world_query.world);
#endif
        CHECK(okay);
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 && !strcmp(marker, footer));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched %zu owner-follow words, %u updates, %zu boundaries, %zu failures\n",
           matched, OWNER_FOLLOW_SCENES * OWNER_FOLLOW_STEPS, boundaries, failures);
    return true;
}

#if defined(AB_TEST_CAMERA_FOLLOW_END_SEQUENCE)
#include "camera_follow_end_sequence_main.h"
#else
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = reference(file, NULL);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}

#endif
