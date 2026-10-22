#include "ab/camera_system.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera system failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue numeric(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

#include "bird_camera_behavior.h"

static void sequence_snapshot(const AbCameraSequenceManager *manager,
                              uint32_t output[6]) {
    output[0] = manager->current != NULL;
    output[1] = bits((float)manager->stage_index);
    output[2] = bits(manager->timer);
    output[3] = manager->stage_timer_present ? 3u : 0u;
    output[4] = manager->stage_timer_present ? bits(manager->stage_timer) : 0;
    output[5] = manager->done_callback;
}

typedef struct {
    uint32_t done_count;
    uint32_t last_done[6];
} Boundaries;

static bool done(void *context, AbCameraSystem *system, AbError *error) {
    (void)error;
    Boundaries *boundaries = context;
    ++boundaries->done_count;
    sequence_snapshot(ab_camera_system_view(system).sequence, boundaries->last_done);
    return true;
}

static bool edge_checks(void) {
    AbCameraSnap snaps[] = {{{0, -50}, 1}, {{100, -100}, 2}};
    AbCameraSystem *system = NULL;
    CHECK(
        !ab_camera_system_open(snaps, 2, (AbVec2){20, 20}, 0, 200, 0, &system, NULL) &&
        !system);
    CHECK(!ab_camera_system_open(NULL, 2, (AbVec2){20, 20}, 0, 200, SIZE_MAX, &system,
                                 NULL) &&
          !system);
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){20, 20}, 0, 200, SIZE_MAX, &system,
                                NULL));
    AbCameraSystemView view = ab_camera_system_view(system);
    CHECK(view.mover && view.limits && view.sequence && !view.viewport &&
          view.input_enabled && view.limits->enabled && !view.automatic_present &&
          view.mover->current_zoom == 1);
    AbDataEntry entries[] = {{string("snapPoint"), string("castle")},
                             {string("zoom"), numeric(5)},
                             {string("validateValues"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable properties = {entries, 3, 3, NULL};
    AbCameraSequenceStage stage = {.action = string("moveToSnapPoint").string,
                                   .properties = &properties,
                                   .duration_present = true};
    AbCameraSequence sequence = {&stage, 1};
    Boundaries boundaries = {0};
    CHECK(ab_camera_system_start(system, &sequence, done, &boundaries, NULL) &&
          ab_camera_system_view(system).automatic_present);
    AbCameraSystemInput input = {.targets_present = true,
                                 .targets = {.values = {{-1000, 100}, 0.01f, 7}}};
    CHECK(ab_camera_system_update(system, 0, &input, NULL));
    view = ab_camera_system_view(system);
    CHECK(view.mover->current_position.x == 100 &&
          view.mover->current_position.y == -100 && view.mover->current_zoom == 2 &&
          view.sequence->current && !view.automatic_present && view.viewport);
    CHECK(ab_camera_system_update(system, 0.125f, &(AbCameraSystemInput){0}, NULL) &&
          boundaries.done_count == 1 && boundaries.last_done[0] == 1 &&
          boundaries.last_done[1] == bits(2) &&
          boundaries.last_done[4] == bits(0.125f));
    /* Missing snap zoom retains current zoom; a numeric selector is one based. */
    entries[0].value = numeric(1);
    entries[1].value.kind = AB_DATA_NIL;
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          ab_camera_system_update(system, 0, &(AbCameraSystemInput){0}, NULL));
    view = ab_camera_system_view(system);
    CHECK(view.mover->current_position.x == 0 && view.mover->current_zoom == 2);
    /* Position actions do not pass validateValues through gotoTargetPosition. */
    AbDataEntry coordinates[] = {{string("x"), numeric(-10000)}};
    AbDataTable position = {coordinates, 1, 1, NULL};
    AbDataEntry position_fields[] = {
        {string("position"), {.kind = AB_DATA_TABLE, .table = &position}},
        {string("validateValues"), {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataTable position_properties = {position_fields, 2, 2, NULL};
    stage = (AbCameraSequenceStage){.action = string("moveToPosition").string,
                                    .properties = &position_properties,
                                    .duration_present = true};
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          ab_camera_system_update(system, 0, &(AbCameraSystemInput){0}, NULL));
    view = ab_camera_system_view(system);
    CHECK(view.mover->current_position.x == -10000 &&
          view.mover->current_position.y == -50);
    coordinates[0].value = numeric(-7000);
    stage.duration_present = false;
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          ab_camera_system_update(system, 1.5f, &(AbCameraSystemInput){0}, NULL));
    view = ab_camera_system_view(system);
    CHECK(view.mover->movement_duration == 3 &&
          view.mover->current_position.x == -8500);
    AbCameraSequenceStage switches[] = {{.action = string("disableInput").string},
                                        {.action = string("unlockCameraLimits").string},
                                        {.action = string("enableInput").string},
                                        {.action = string("lockCameraLimits").string}};
    sequence = (AbCameraSequence){switches, 4};
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          !ab_camera_system_view(system).input_enabled);
    CHECK(ab_camera_system_update(system, 0, &(AbCameraSystemInput){0}, NULL) &&
          !ab_camera_system_view(system).limits->enabled);
    CHECK(ab_camera_system_update(system, 0, &(AbCameraSystemInput){0}, NULL) &&
          ab_camera_system_view(system).input_enabled);
    CHECK(ab_camera_system_update(system, 0, &(AbCameraSystemInput){0}, NULL) &&
          ab_camera_system_view(system).limits->enabled);
    input = (AbCameraSystemInput){
        .targets_present = true,
        .targets.values = {.zoom = NAN, .present = AB_CAMERA_VALUE_ZOOM}};
    CHECK(ab_camera_system_update(system, 0, &input, NULL) &&
          ab_camera_system_view(system).mover->current_zoom ==
              ab_camera_system_view(system).limits->close_zoom);
    stage = (AbCameraSequenceStage){.action = string("followBird").string};
    sequence = (AbCameraSequence){&stage, 1};
    CHECK(!ab_camera_system_start(system, &sequence, NULL, NULL, NULL));
    CHECK(!ab_camera_system_start(NULL, &sequence, NULL, NULL, NULL) &&
          !ab_camera_system_update(system, 0, NULL, NULL) &&
          !ab_camera_system_view(NULL).mover);
    ab_camera_system_close(system);
    ab_camera_system_close(NULL);
    puts("Camera target replacement, snap predicates, position defaults, callback "
         "order, input switches and ownership bounds pass.");
    return true;
}

static bool velocity_checks(void) {
    AbCameraSnap snaps[] = {{{0, 0}, 1}, {{3, 4}, 2}};
    AbCameraSystem *system = NULL;
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){20, 20}, 0, 200, SIZE_MAX, &system,
                                NULL));
    AbDataEntry entries[] = {{string("snapPoint"), string("castle")},
                             {string("velocity"), numeric(2)},
                             {string("validateValues"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable properties = {entries, 3, 3, NULL};
    AbCameraSequenceStage stage = {.action = string("moveToSnapPoint").string,
                                   .properties = &properties};
    AbCameraSequence sequence = {&stage, 1};
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL));
    CHECK(stage.duration_present && stage.duration == 2.5f &&
          stage.velocity_duration == 2.5f &&
          stage.condition == AB_CAMERA_CONDITION_VELOCITY);
    CHECK(ab_camera_system_update(system, 1, &(AbCameraSystemInput){0}, NULL));
    CHECK(ab_camera_system_view(system).mover->movement_duration == 2.5f);
    /* The original closure captures duration rather than re-reading it. */
    stage.duration = 0;
    CHECK(ab_camera_system_update(system, 1.5f, &(AbCameraSystemInput){0}, NULL) &&
          ab_camera_system_view(system).sequence->current == &sequence);
    CHECK(ab_camera_system_update(system, 0.0001f, &(AbCameraSystemInput){0}, NULL) &&
          !ab_camera_system_view(system).sequence->current);
    entries[1].value = numeric(0);
    stage.condition = AB_CAMERA_CONDITION_UNSET;
    stage.duration = 1;
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          stage.condition == AB_CAMERA_CONDITION_DURATION && stage.duration == 1);
    ab_camera_system_close(system);
    return true;
}

typedef struct {
    AbCameraFollowBird bird;
    size_t calls;
    bool fail;
} FollowResolverTest;

static bool follow_resolver(void *context, AbDataValue target,
                            const AbCameraFollowBird **bird,
                            AbCameraFollowHost **environment, AbError *error) {
    FollowResolverTest *test = context;
    ++test->calls;
    *environment = NULL;
    if (test->fail)
        return ab_fail(error, "Reject follow-target resolution");
    if (target.kind == AB_DATA_NIL) {
        *bird = NULL;
        return true;
    }
    CHECK(target.kind == AB_DATA_STRING && target.string.size == 10 &&
          !memcmp(target.string.data, "flyingBird", 10));
    *bird = &test->bird;
    return true;
}

static bool follow_resolver_checks(void) {
    AbCameraSnap snaps[] = {{{0, 0}, 2}, {{500, 0}, 1}};
    AbCameraSystem *system = NULL;
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){320, 180}, 1000, 50, SIZE_MAX,
                                &system, NULL));
    AbDataEntry entries[] = {{string("target"), string("flyingBird")},
                             {string("velocityCompensation"), numeric(0)}};
    AbDataTable properties = {entries, 2, 2, NULL};
    AbCameraSequenceStage stage = {.action = string("followTarget").string,
                                   .properties = &properties};
    AbCameraSequence sequence = {&stage, 1};
    CHECK(!ab_camera_system_start(system, &sequence, NULL, NULL, NULL));
    FollowResolverTest test = {0};
    CHECK(ab_camera_system_bind_follow(system, &test, follow_resolver) &&
          ab_camera_system_start(system, &sequence, NULL, NULL, NULL));
    AbCameraSystemView view = ab_camera_system_view(system);
    CHECK(test.calls == 1 && view.follow && !view.follow->allow_cancel &&
          view.follow->velocity_compensation == 0 &&
          ab_camera_system_target_bird(system) == &test.bird);
    stage.interrupt_on_input = true;
    entries[1].value.kind = AB_DATA_NIL;
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL));
    view = ab_camera_system_view(system);
    CHECK(view.follow && view.follow->allow_cancel &&
          view.follow->velocity_compensation == 0.7f);
    test.fail = true;
    CHECK(!ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          ab_camera_system_target_bird(system) == &test.bird);
    test.fail = false;
    entries[0].value.kind = AB_DATA_NIL;
    entries[1].value = (AbDataValue){.kind = AB_DATA_TABLE, .table = &properties};
    CHECK(ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          !ab_camera_system_view(system).automatic_present &&
          !ab_camera_system_target_bird(system));
    CHECK(ab_camera_system_bind_follow(system, NULL, NULL) &&
          !ab_camera_system_start(system, &sequence, NULL, NULL, NULL) &&
          !ab_camera_system_bind_follow(NULL, NULL, NULL));
    ab_camera_system_close(system);
    return true;
}

static bool cancellation_checks(void) {
    AbCameraSnap snaps[] = {{{0, 0}, 2}, {{500, 0}, 1}};
    AbCameraSystem *system = NULL;
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){320, 180}, 1000, 50, SIZE_MAX,
                                &system, NULL));
    AbCameraSequenceStage stage = {
        .action = string("wait").string, .duration = 1000, .duration_present = true};
    AbCameraSequence sequence = {&stage, 1};
    Boundaries boundaries = {0};
    CHECK(ab_camera_system_start(system, &sequence, done, &boundaries, NULL) &&
          ab_camera_system_goto_snap(system, 0, true, true, NULL, NULL) &&
          ab_camera_system_cancel_sequence(system, NULL));
    AbCameraSystemView view = ab_camera_system_view(system);
    CHECK(!view.sequence->current && view.automatic_present &&
          boundaries.done_count == 1 && boundaries.last_done[0] == 1 &&
          boundaries.last_done[1] == bits(1));
    CHECK(ab_camera_system_cancel_sequence(system, NULL) &&
          boundaries.done_count == 1 && !ab_camera_system_cancel_sequence(NULL, NULL));
    ab_camera_system_close(system);
    return true;
}

static void mover_snapshot(const AbCameraMover *mover, uint32_t output[16]) {
    output[0] = (mover->target_position_present ? 1u : 0u) |
                (mover->target_zoom_present ? 2u : 0u) |
                (mover->start_position_present ? 4u : 0u) |
                (mover->start_zoom_scale_present ? 8u : 0u) |
                (mover->start_zoom_present ? 16u : 0u);
    const float values[] = {
        mover->current_position.x,
        mover->current_position.y,
        mover->target_position_present ? mover->target_position.x : 0,
        mover->target_position_present ? mover->target_position.y : 0,
        mover->start_position_present ? mover->start_position.x : 0,
        mover->start_position_present ? mover->start_position.y : 0,
        mover->current_zoom,
        mover->target_zoom_present ? mover->target_zoom : 0,
        mover->zoom_velocity,
        mover->start_zoom_scale_present ? mover->start_zoom_scale : 0,
        mover->start_zoom_present ? mover->start_zoom : 0,
        mover->movement_duration,
        mover->movement_timer,
        mover->zoom_duration,
        mover->zoom_timer};
    for (size_t index = 0; index < 15; ++index) {
        output[index + 1] = bits(values[index]);
    }
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static void snapshot(AbCameraSystem *system, uint32_t publications,
                     const Boundaries *boundaries, uint32_t output[46]) {
    AbCameraSystemView view = ab_camera_system_view(system);
    mover_snapshot(view.mover, output);
    sequence_snapshot(view.sequence, output + 16);
    output[22] = view.limits->enabled;
    output[23] = view.automatic_present;
    output[24] = view.input_enabled;
    if (view.viewport) {
        output[25] = bits(view.viewport->rendering.top_left.x);
        output[26] = bits(view.viewport->rendering.top_left.y);
        output[27] = bits(view.viewport->rendering.zoom.x);
    }
    output[28] = publications;
    const float limits[] = {view.limits->left,
                            view.limits->right,
                            view.limits->top,
                            view.limits->bottom,
                            view.limits->close_zoom,
                            view.limits->far_zoom,
                            view.limits->absolute_bottom,
                            view.limits->horizontal_leeway};
    for (size_t field = 0; field < 8; ++field) {
        output[29 + field] = bits(limits[field]);
    }
    output[37] = boundaries->done_count;
    memcpy(output + 38, boundaries->last_done, sizeof(boundaries->last_done));
    output[44] = bits(view.mover->current_position.x);
    output[45] = bits(view.mover->current_position.y);
}

static bool compare_case(FILE *file, AbCameraSequence sources[2], size_t item) {
    uint32_t setup[10];
    CHECK(words(file, setup, 10));
    AbCameraSnap snaps[2] = {{{number(setup[4]), number(setup[5])}, number(setup[6])},
                             {{number(setup[7]), number(setup[8])}, number(setup[9])}};
    AbCameraSystem *system = NULL;
    AbError error = {0};
    bool okay = false;
    Boundaries boundaries = {0};
    uint32_t publications = 0;
    if (!ab_camera_system_open(snaps, 2, (AbVec2){number(setup[0]), number(setup[1])},
                               number(setup[2]), number(setup[3]), SIZE_MAX, &system,
                               &error)) {
        goto release_system;
    }
    for (size_t step = 0; step < 128; ++step) {
        uint32_t command[12], expected[46], actual[46] = {0};
        if (!words(file, command, 12) || !words(file, expected, 46)) {
            goto release_system;
        }
        bool applied;
        if (command[0] < 2) {
            applied =
                ab_camera_system_start(system, sources + command[0],
                                       command[11] ? done : NULL, &boundaries, &error);
        } else {
            AbCameraSystemInput input = {
                .targets = {.values = {{number(command[4]), number(command[5])},
                                       number(command[6]),
                                       command[3]},
                            .movement_duration = number(command[7]),
                            .zoom_duration = number(command[8])},
                .shake = {number(command[9]), number(command[10])},
                .targets_present = (command[2] & 8) != 0,
                .user_input = (command[2] & 1) != 0,
                .moving_objects = (command[2] & 2) != 0,
                .specialty_available = (command[2] & 4) != 0};
            applied =
                ab_camera_system_update(system, number(command[1]), &input, &error);
            ++publications;
        }
        if (!applied) {
            fprintf(stderr, "Camera system case %zu step %zu: %s\n", item, step,
                    error.message);
            goto release_system;
        }
        snapshot(system, publications, &boundaries, actual);
        for (size_t field = 0; field < 46; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Camera system case %zu step %zu field %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        item, step, field, actual[field], expected[field]);
                goto release_system;
            }
        }
    }
    okay = true;

release_system:
    ab_camera_system_close(system);
    return okay;
}

static bool compare_native(FILE *file, AbCameraSequence sources[2]) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_CAMERA_SYSTEM_V1 64 128\n") == 0);
    for (size_t item = 0; item < 64; ++item) {
        CHECK(compare_case(file, sources, item));
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera system reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 8192 integrated original camera commands match: APK default/restart "
         "stages, automatic replacement, limits, mover, callback state and native "
         "viewport publication.");
    return true;
}

static bool apk_reference(const char *apk, const char *reference) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData data[2] = {0};
    AbCameraSequence sources[2] = {0};
    uint8_t key[32] = {0};
    FILE *file = NULL;
    bool okay = false;
    AbError error = {0};
    if (!ab_file_read(apk, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error)) {
        goto release_assets;
    }
    const char *paths[] = {
        "assets/data/scripts/cameraSequences/DefaultLevelStartCameraSequence.lua",
        "assets/data/scripts/cameraSequences/RestartLevelCameraSequence.lua"};
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_target_data_open(&archive, paths[index], (AbSpan){key, sizeof(key)},
                                 1024 * 1024, NULL, data + index, &error)) {
            goto release_assets;
        }
        const AbDataValue *source =
            ab_data_get(data[index].data.globals, string("sequence"));
        if (!source || source->kind != AB_DATA_TABLE ||
            !ab_camera_sequence_read(source->table, 65536, sources + index, &error)) {
            goto release_assets;
        }
    }
    file = fopen(reference, "rb");
    okay = file && compare_native(file, sources);

release_assets:
    memset(key, 0, sizeof(key));
    if (file && fclose(file) != 0) {
        okay = false;
    }
    for (size_t index = 0; index < 2; ++index) {
        ab_camera_sequence_free(sources + index);
        ab_target_data_close(data + index);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() && velocity_checks() && follow_resolver_checks() &&
                       bird_camera_behavior_checks() && cancellation_checks()
                   ? 0
                   : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--apk") == 0) {
        return apk_reference(argv[2], argv[3]) ? 0 : 1;
    }
    return 2;
}
