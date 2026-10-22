#include "ab/classic_camera.h"
#include "ab/target_data.h"
#include "classic_camera_cases.h"
#include "camera_state_words.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Classic camera check failed at %d: %s\n", __LINE__,       \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

#include "classic_camera_level.h"

typedef struct {
    ClassicCameraLevelInput data;
    AbClassicLevel *level;
    AbClassicLevelView level_view;
    AbClassicCamera *camera;
    AbCameraSnap snaps[2];
    const AbCameraSequence *sources;
    size_t indices[2];
    float gravity;
    uint32_t config[24], command[12];
    uint32_t events[OWNER_FOLLOW_MAX_EVENTS][16], counts[12], count;
    bool live;
} ClassicCameraFixture;

static bool same_text(AbSpan value, const char *text) {
    return value.size == strlen(text) &&
           (!value.size || (value.data && !memcmp(value.data, text, value.size)));
}

static uint32_t *record(ClassicCameraFixture *fixture, uint32_t kind, uint32_t pc,
                        uint32_t size) {
    if (fixture->count >= OWNER_FOLLOW_MAX_EVENTS || kind >= 12)
        return NULL;
    uint32_t *row = fixture->events[fixture->count++];
    memset(row, 0, 16 * sizeof(*row));
    row[0] = kind;
    row[1] = ++fixture->counts[kind];
    row[2] = pc;
    row[3] = size;
    return row;
}

static bool load_sequence(void *context, const char *name,
                          const AbCameraSequence **output, AbError *error) {
    (void)error;
    ClassicCameraFixture *fixture = context;
    uint32_t resource = !strcmp(name, "FollowShotBirdEndCameraSequence") ? 1u
                        : !strcmp(name, "FollowShotBirdCameraSequence")  ? 2u
                                                                         : 0u;
    CHECK(resource);
    uint32_t *row = record(fixture, 4, 9, 19);
    CHECK(row);
    row[4] = resource;
    row[6] = 1;
    if (fixture->live) {
        size_t current = ab_classic_camera_view(fixture->camera).flying_object;
        CHECK(current != SIZE_MAX);
        const AbDataTable *physical =
            &fixture->level_view.properties->objects[current].physical;
        const AbDataValue *launch =
            ab_data_get(physical, classic_camera_text("launchVelocity"));
        row[7] = launch && launch->kind != AB_DATA_NIL ? 5u : 0u;
        CHECK(!launch || launch->kind == AB_DATA_NIL || launch->kind == AB_DATA_TABLE);
        row[8] = current == fixture->indices[0]   ? 1u
                 : current == fixture->indices[1] ? 2u
                                                  : 0u;
        CHECK(row[8]);
    }
    *output = fixture->sources + (resource - 1u);
    return true;
}

static bool sequence_log(void *context, AbSpan message, AbError *error) {
    (void)error;
    ClassicCameraFixture *fixture = context;
    uint32_t kind =
        same_text(message, "<CameraSequenceManager> starting new camera sequence") ? 2u
        : same_text(message, "<CameraSequenceManager> loading sequence from file: "
                             "FollowShotBirdEndCameraSequence")
            ? 1u
        : same_text(message, "<CameraSequenceManager> loading sequence from file: "
                             "FollowShotBirdCameraSequence")
            ? 3u
            : 0u;
    CHECK(kind);
    uint32_t *row = record(fixture, 7, kind == 2 ? 3u : 15u, kind == 2 ? 24u : 19u);
    CHECK(row);
    row[4] = kind;
    return true;
}

static bool prediction_print(void *context, size_t rays, AbError *error) {
    (void)error;
    ClassicCameraFixture *fixture = context;
    uint32_t *row = record(fixture, 11, 109, 115);
    CHECK(row && rays <= UINT32_C(0x1000000));
    row[4] = owner_follow_word((float)rays);
    return true;
}

static bool observe_query(void *context, AbVec2 first, AbVec2 second,
                          const AbLevelRaycast *result, AbError *error) {
    (void)error;
    ClassicCameraFixture *fixture = context;
    uint32_t *row = record(fixture, 9, 19, 22);
    CHECK(row);
    row[4] = owner_follow_word(first.x);
    row[5] = owner_follow_word(first.y);
    row[6] = owner_follow_word(second.x);
    row[7] = owner_follow_word(second.y);
    row[8] = result->hit ? 6u : 1u;
    row[9] = result->hit;
    if (result->hit) {
        /* The native fixture has zero userdata. C retains its canonical name;
         * Follow never reads prediction names. Compare the shared numeric hit. */
        CHECK(result->name.kind == AB_DATA_STRING);
        row[10] = owner_follow_word(result->point.x);
        row[11] = owner_follow_word(result->point.y);
        row[12] = owner_follow_word(result->normal.x);
        row[13] = owner_follow_word(result->normal.y);
    }
    return true;
}

static bool open_fixture(ClassicCameraFixture *fixture, uint32_t scene) {
    camera_follow_system_config(scene, fixture->config);
    CHECK(classic_camera_open_level(&fixture->data, &fixture->level));
    fixture->level_view = ab_classic_level_view(fixture->level);
    CHECK(fixture->level_view.bodies->count == CLASSIC_CAMERA_BODIES);
    fixture->indices[0] = fixture->indices[1] = SIZE_MAX;
    for (size_t index = 0; index < fixture->level_view.bodies->count; ++index) {
        AbSpan name = fixture->level_view.bodies->objects[index].name;
        if (same_text(name, "bird1"))
            fixture->indices[0] = index;
        else if (same_text(name, "bird2"))
            fixture->indices[1] = index;
    }
    CHECK(fixture->indices[0] != SIZE_MAX && fixture->indices[1] != SIZE_MAX);
    const uint32_t *input = fixture->config;
    fixture->gravity = owner_follow_number(input[16]);
    for (size_t index = 0; index < 2; ++index) {
        size_t snap = index ? 6u : 3u;
        fixture->snaps[index] = (AbCameraSnap){
            {owner_follow_number(input[snap]), owner_follow_number(input[snap + 1])},
            owner_follow_number(input[snap + 2])};
        size_t first = index ? 19u : 9u;
        size_t object = fixture->indices[index];
        fixture->level_view.character_objects[object].jump.position = (AbVec2){
            owner_follow_number(input[first]), owner_follow_number(input[first + 1])};
        if (!fixture->live && (index || !(input[18] & 2u))) {
            CHECK(ab_level_properties_publish_launch(
                fixture->level_view.properties, object,
                (AbVec2){owner_follow_number(input[first + 2]),
                         owner_follow_number(input[first + 3])},
                NULL));
        }
        CHECK(ab_level_property_set(
            fixture->level_view.properties, object,
            (AbSpan){(const uint8_t *)"radius", 6},
            classic_camera_number(0.25f + (float)(input[0] % 4u) * 0.125f), NULL));
    }
    fixture->level_view.world->locked = (input[0] & 16u) != 0;
    AbClassicCameraConfig config = {
        .level = fixture->level,
        .snaps = fixture->snaps,
        .snap_count = 2,
        .pixel_size = {owner_follow_number(input[1]), owner_follow_number(input[2])},
        .absolute_bottom = 200,
        .horizontal_leeway = 200,
        .gravity = &fixture->gravity,
        .shot_sequence = fixture->sources + 1,
        .end_sequence = fixture->sources,
        .resource_context = fixture,
        .load_sequence = load_sequence,
        .sequence_log = sequence_log,
        .prediction = {fixture, 2048, prediction_print, observe_query},
        .storage_bytes = 65536};
    return ab_classic_camera_open(&config, &fixture->camera, NULL);
}

static bool publish_inputs(ClassicCameraFixture *fixture) {
    for (uint32_t role = 0; role < 2; ++role) {
        uint32_t values[5];
        classic_camera_frame_values(fixture->config, fixture->command[11], role,
                                    values);
        size_t index = fixture->indices[role];
        fixture->level_view.character_objects[index].jump.position =
            (AbVec2){owner_follow_number(values[0]), owner_follow_number(values[1])};
        AbLevelPropertyObject *object = fixture->level_view.properties->objects + index;
        if (values[4]) {
            if (!object->launch_published) {
                CHECK(ab_level_properties_publish_launch(
                    fixture->level_view.properties, index,
                    (AbVec2){owner_follow_number(values[2]),
                             owner_follow_number(values[3])},
                    NULL));
            } else {
                object->launch_entries[0].value.bits = values[2];
                object->launch_entries[1].value.bits = values[3];
            }
        } else {
            CHECK(!object->launch_published);
        }
    }
    return true;
}

static bool action(ClassicCameraFixture *fixture) {
    AbCameraSystem *system = ab_classic_camera_system(fixture->camera);
    const uint32_t *command = fixture->command;
    uint32_t operation = command[0];
    if (operation == 1 || operation == 2) {
        CHECK(ab_classic_camera_set_flying(fixture->camera,
                                           fixture->indices[operation - 1u], NULL));
        return fixture->config[0] % 3u == 2
                   ? ab_classic_camera_blues_initialize(fixture->camera, NULL)
                   : ab_classic_camera_on_launch(fixture->camera, NULL);
    }
    if (operation == 3 || operation == 4)
        return ab_classic_camera_remove_bird(fixture->camera,
                                             fixture->indices[operation - 3u], NULL);
    if (operation == 5)
        return ab_camera_system_follow_bird(system, NULL, NULL, NULL, NULL, NULL);
    if (operation == 8)
        return ab_camera_system_remove_bird(system, NULL);
    if (operation == 10)
        return ab_classic_camera_start_sequence(
            fixture->camera, "FollowShotBirdEndCameraSequence", NULL);
    AbCameraMove move = {.duration = owner_follow_number(command[7]),
                         .duration_present = true};
    if (operation == 6 || operation == 9) {
        AbCameraValues values = {.position = {owner_follow_number(command[4]),
                                              owner_follow_number(command[5])},
                                 .zoom = owner_follow_number(command[6]),
                                 .present = command[3]};
        CHECK(ab_camera_system_goto_position(system, &values, &move, NULL));
        return operation != 9 || ab_camera_system_remove_bird(system, NULL);
    }
    if (operation == 7)
        return ab_camera_system_goto_snap(system, 0, true, true, &move, NULL);
    return operation == 0;
}

static bool snapshot(ClassicCameraFixture *fixture, uint32_t output[80]) {
    AbClassicCameraView view = ab_classic_camera_view(fixture->camera);
    uint32_t target = view.target_object == fixture->indices[0]   ? 1u
                      : view.target_object == fixture->indices[1] ? 2u
                                                                  : 0u;
    CHECK(camera_state_words(view.system, target, output));
    CHECK(camera_sequence_words(view.system.sequence, view.loaded_sequence, true,
                                output));
    output[47] = fixture->counts[4];
    output[50] = fixture->count;
    output[63] = fixture->command[11];
    output[79] = fixture->counts[7];
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t scene,
                  uint32_t step, const char *phase) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[index]) {
            fprintf(stderr,
                    "Classic camera scene %u step %u %s word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    scene, step, phase, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool compare_scene(FILE *file, ClassicCameraFixture *fixture, uint32_t scene,
                          size_t *matched, size_t *boundaries, size_t *failures) {
    CHECK(words(file, fixture->config, 24, scene, 0, "config"));
    *matched += 24;
    for (size_t index = 0; index < CLASSIC_CAMERA_BODIES; ++index) {
        AbSpan name = fixture->level_view.bodies->objects[index].name;
        uint32_t role = same_text(name, "bird1")   ? 0u
                        : same_text(name, "bird2") ? 1u
                                                   : 2u;
        CHECK(role != 2 || same_text(name, "ground"));
        uint32_t row[13];
        classic_camera_body(role, row);
        CHECK(words(file, row, 13, scene, (uint32_t)index, "constructor body"));
        *matched += 13;
    }
    for (uint32_t step = 0; step < OWNER_FOLLOW_STEPS; ++step) {
        camera_follow_system_command(scene, step, fixture->command);
        CHECK(words(file, fixture->command, 12, scene, step, "command"));
        fixture->count = 0;
        CHECK(action(fixture));
        if (fixture->live)
            CHECK(publish_inputs(fixture));
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
        bool okay = ab_classic_camera_update(
            fixture->camera, owner_follow_number(command[1]), &input, &error);
        uint32_t output[80] = {0};
        output[0] = !okay;
        CHECK(snapshot(fixture, output));
        if (!words(file, output, 80, scene, step, "state")) {
            if (error.message[0])
                fprintf(stderr, "%s\n", error.message);
            return false;
        }
        for (uint32_t index = 0; index < fixture->count; ++index)
            CHECK(words(file, fixture->events[index], 16, scene, step, "service"));
        *matched += 12 + 80 + (size_t)fixture->count * 16;
        *boundaries += fixture->count;
        *failures += !okay;
    }
    return true;
}

static bool reference(FILE *file, const AbCameraSequence sources[2]) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file));
    bool live = !strcmp(marker, "AB_ANDROID_CLASSIC_CAMERA_V2 48 160 3\n");
    CHECK(live || !strcmp(marker, "AB_ANDROID_CLASSIC_CAMERA_V1 48 160 3\n"));
    size_t matched = 0, boundaries = 0, failures = 0;
    for (uint32_t scene = 0; scene < OWNER_FOLLOW_SCENES; ++scene) {
        ClassicCameraFixture fixture = {.sources = sources, .live = live};
        bool okay =
            open_fixture(&fixture, scene) &&
            compare_scene(file, &fixture, scene, &matched, &boundaries, &failures);
        ab_classic_camera_close(fixture.camera);
        ab_classic_level_close(fixture.level);
        CHECK(okay);
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 &&
          !strcmp(marker, "AB classic camera reference complete") &&
          fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched %zu owned level-camera words, %u updates, %zu service boundaries, "
           "%zu failures\n",
           matched, OWNER_FOLLOW_SCENES * OWNER_FOLLOW_STEPS, boundaries, failures);
    return true;
}

static bool apk_reference(const char *apk, const char *trace) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData data[2] = {0};
    AbCameraSequence sources[2] = {0};
    uint8_t key[32] = {0};
    FILE *file = NULL;
    bool okay = false;
    AbError error = {0};
    const char *const paths[] = {
        "assets/data/scripts/cameraSequences/FollowShotBirdEndCameraSequence.lua",
        "assets/data/scripts/cameraSequences/FollowShotBirdCameraSequence.lua"};
    if (!ab_file_read(apk, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error))
        goto release_assets;
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_target_data_open(&archive, paths[index], (AbSpan){key, sizeof(key)},
                                 1024 * 1024, NULL, data + index, &error))
            goto release_assets;
        const AbDataValue *sequence =
            ab_data_get(data[index].data.globals, classic_camera_text("sequence"));
        if (!sequence || sequence->kind != AB_DATA_TABLE ||
            !ab_camera_sequence_read(sequence->table, 65536, sources + index, &error))
            goto release_assets;
    }
    file = fopen(trace, "rb");
    okay = file && reference(file, sources);

release_assets:
    memset(key, 0, sizeof(key));
    if (file && fclose(file))
        okay = false;
    for (size_t index = 0; index < 2; ++index) {
        ab_camera_sequence_free(sources + index);
        ab_target_data_close(data + index);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    return apk_reference(argv[1], argv[2]) ? 0 : 1;
}
