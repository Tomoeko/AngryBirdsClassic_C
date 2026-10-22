#include "ab/classic_start_level.h"
#include "ab/camera_bootstrap.h"
#include "ab/target_data.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Start-level check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            goto release;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbClassicStartLevel *start;
    AbClassicCamera *camera;
    AbCameraTouchHandler input;
    AbCameraTouchShared shared;
    AbCameraShake shake;
    AbDataValue gestures, editor, level;
    AbEventListener listener;
    const AbDataTable *ids;
    AbCameraSequence sequences[4];
    AbClassicLoaderCall hook;
    AbDataEntry script_entry, script_method;
    AbDataTable scripts, script, campaigns;
    AbDataTable *notices[16];
    size_t count, logs, loaded, hooks;
    unsigned phase;
    bool skip_game;
} Fixture;

static const char *const sequence_names[] = {
    "FollowShotBirdCameraSequence", "FollowShotBirdEndCameraSequence",
    "DefaultLevelStartCameraSequence", "RestartLevelCameraSequence"};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue field(const AbDataTable *table, const char *key) {
    const AbDataValue *value = ab_data_get(table, text(key));
    return value ? *value : (AbDataValue){0};
}

static bool equal(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN}};
    AbDataTable table = {&entry, 1, 1, NULL};
    return ab_data_get(&table, right) != NULL;
}

static bool load_sequence(void *context, const char *name,
                          const AbCameraSequence **output, AbError *error) {
    Fixture *fixture = context;
    if (fixture->phase != 1)
        return ab_fail(error, "Camera sequence preceded initialized notification");
    for (size_t index = 0; index < 4; ++index) {
        if (!strcmp(sequence_names[index], name)) {
            fixture->loaded = index;
            *output = fixture->sequences + index;
            return true;
        }
    }
    return ab_fail(error, "Unknown startup sequence");
}

static bool log_sequence(void *context, AbSpan message, AbError *error) {
    Fixture *fixture = context;
    if (fixture->phase != 1 || !message.size)
        return ab_fail(error, "Camera startup diagnostic order differs");
    ++fixture->logs;
    return true;
}

static bool event(void *context, AbDataTable *notice, AbError *error) {
    Fixture *fixture = context;
    const char *const ids[] = {"EID_CAMERAS_INITIALISED", "EID_LEVEL_LOADING_DONE",
                               "EID_GOTO_GAME"};
    if (fixture->phase >= 3 ||
        !equal(field(notice, "id"), field(fixture->ids, ids[fixture->phase])) ||
        fixture->count == 16 || ab_classic_start_level_call(fixture->start, NULL))
        return ab_fail(error, "Startup event order, prefix or recursion differs");
    AbCameraSystemView camera = ab_classic_camera_view(fixture->camera).system;
    if (!fixture->phase) {
        if (!camera.input_enabled ||
            camera.mover->movement_tween != AB_CAMERA_TWEEN_CUBIC_IN_OUT ||
            camera.mover->zoom_tween != AB_CAMERA_TWEEN_CUBIC_IN_OUT ||
            fixture->shared.tap_count != 0 || fixture->shake.duration != 0 ||
            fixture->shake.amount != 0 || fixture->shake.offset.x != 3)
            return ab_fail(error, "Camera initialization fields differ");
        if (fixture->skip_game)
            fixture->editor = (AbDataValue){.kind = AB_DATA_NUMBER};
    }
    if (fixture->phase == 1)
        fixture->level = text("Level57");
    for (size_t index = 0; index < fixture->count; ++index)
        if (fixture->notices[index] == notice)
            return ab_fail(error, "Startup event identity was reused");
    fixture->notices[fixture->count++] = notice;
    ++fixture->phase;
    return true;
}

static bool hook(void *context, AbError *error) {
    Fixture *fixture = context;
    if (fixture->phase != (fixture->skip_game ? 2u : 3u) ||
        !equal(fixture->level, text("Level57")))
        return ab_fail(error,
                       "Level hook lookup did not follow live notification writes");
    ++fixture->hooks;
    fixture->phase = 4;
    return true;
}

static bool check(const char *apk) {
    Fixture fixture = {0};
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbClassicLevel *level = NULL;
    AbEventManager *events = NULL;
    AbTargetData source[4] = {0}, ids = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = false;
    CHECK(ab_file_read(apk, 64 * 1024 * 1024, &bytes, &error) &&
          ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
          ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
          ab_target_data_open(&archive, "assets/data/scripts_common/events.lua",
                              (AbSpan){key, sizeof(key)}, 1024 * 1024, NULL, &ids,
                              &error));
    fixture.ids = ids.data.globals;
    size_t sequence_capacity = 0;
    for (size_t index = 0; index < 4; ++index) {
        char path[160];
        int count =
            snprintf(path, sizeof(path), "assets/data/scripts/cameraSequences/%s.lua",
                     sequence_names[index]);
        CHECK(count > 0 && (size_t)count < sizeof(path) &&
              ab_target_data_open(&archive, path, (AbSpan){key, sizeof(key)},
                                  1024 * 1024, NULL, source + index, &error));
        AbDataValue record = field(source[index].data.globals, "sequence");
        CHECK(record.kind == AB_DATA_TABLE &&
              ab_camera_sequence_read(record.table, 65536, fixture.sequences + index,
                                      &error));
        if (fixture.sequences[index].count > sequence_capacity)
            sequence_capacity = fixture.sequences[index].count;
    }
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat",
                           "assets/data/images/base/INGAME_BLOCKS_2.dat"};
    AbClassicLevelAssets assets = {.level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = paths,
                                   .sprite_sheet_count = 4,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true};
    AbWorldDefinition definition = {.broadphase_limits = {128, 1024, 1024},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    CHECK(ab_classic_level_load(&archive, &assets, definition,
                                (AbClassicLevelLimits){.objects = 32,
                                                       .contacts = 128,
                                                       .storage_bytes = 1024 * 1024},
                                true, &level, &error));
    AbClassicLevelView view = ab_classic_level_view(level);
    AbCameraSnap snaps[2];
    float gravity = view.world->definition.gravity.y;
    CHECK(ab_camera_museum_snaps(view.data->globals, (AbVec2){1024, 768}, snaps,
                                 &error) &&
          ab_classic_camera_open(
              &(AbClassicCameraConfig){.level = level,
                                       .snaps = snaps,
                                       .snap_count = 2,
                                       .pixel_size = {1024, 768},
                                       .absolute_bottom = 200,
                                       .horizontal_leeway = 200,
                                       .gravity = &gravity,
                                       .shot_sequence = fixture.sequences,
                                       .end_sequence = fixture.sequences + 1,
                                       .resource_context = &fixture,
                                       .load_sequence = load_sequence,
                                       .sequence_log = log_sequence,
                                       .sequence_capacity = sequence_capacity,
                                       .storage_bytes = 128 * 1024},
              &fixture.camera, &error) &&
          ab_event_manager_open((AbEventManagerLimits){8, 4, 4, 4, 8, 65536},
                                (AbEventManagerHost){0}, &events, &error));
    fixture.listener = (AbEventListener){.context = &fixture, .event_triggered = event};
    const char *const event_names[] = {"EID_CAMERAS_INITIALISED",
                                       "EID_LEVEL_LOADING_DONE", "EID_GOTO_GAME"};
    for (size_t index = 0; index < 3; ++index)
        CHECK(ab_event_manager_add(events, field(fixture.ids, event_names[index]),
                                   &fixture.listener, &error));
    fixture.hook = (AbClassicLoaderCall){&fixture, hook};
    fixture.script_method = (AbDataEntry){
        text("onStartLevel"), {.kind = AB_DATA_OBJECT, .object = &fixture.hook}};
    fixture.script = (AbDataTable){&fixture.script_method, 1, 1, NULL};
    fixture.script_entry = (AbDataEntry){
        text("Level57"), {.kind = AB_DATA_TABLE, .table = &fixture.script}};
    fixture.scripts = (AbDataTable){&fixture.script_entry, 1, 1, NULL};
    fixture.gestures = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_start_level_open(
        (AbClassicStartLevelConfig){.camera = fixture.camera,
                                    .input = &fixture.input,
                                    .shared_input = &fixture.shared,
                                    .shake = &fixture.shake,
                                    .events = events,
                                    .event_ids = fixture.ids,
                                    .level_scripts = &fixture.scripts,
                                    .promo_campaigns = &fixture.campaigns,
                                    .touch_gestures = &fixture.gestures,
                                    .started_from_editor = &fixture.editor,
                                    .level_name = &fixture.level,
                                    .starts = 4,
                                    .storage_bytes = 8192},
        &fixture.start, &error));
    for (unsigned sample = 0; sample < 4; ++sample) {
        fixture.phase = 0;
        fixture.skip_game = sample == 1;
        if (sample == 3)
            fixture.script_method.value = (AbDataValue){.kind = AB_DATA_NUMBER};
        fixture.editor = (AbDataValue){0};
        fixture.level = text("Level1");
        fixture.shared.tap_count = 7;
        fixture.shake = (AbCameraShake){.duration = 1, .amount = 9, .offset = {3, 4}};
        AbCameraSystem *camera = ab_classic_camera_system(fixture.camera);
        CHECK(ab_camera_system_input_enabled(camera, false) &&
              ab_camera_system_movement_tweens(camera, AB_CAMERA_TWEEN_END,
                                               AB_CAMERA_TWEEN_SMOOTHSTEP));
        AbDataValue restart =
            sample == 1 ? (AbDataValue){.kind = AB_DATA_NUMBER} : (AbDataValue){0};
        bool result = sample == 2
                          ? ab_classic_start_level_call(fixture.start, &error)
                          : ab_classic_start_level(fixture.start, restart, &error);
        CHECK(result == (sample != 3) && fixture.loaded == (sample == 1 ? 3u : 2u) &&
              fixture.logs == (sample + 1) * 2 &&
              fixture.hooks == (sample == 3 ? 3u : sample + 1) &&
              fixture.phase == (sample == 3 ? 3u : 4u));
    }
    CHECK(fixture.count == 11 && !ab_classic_start_level_call(fixture.start, NULL) &&
          fixture.hooks == 3);
    puts("Start-level completion composes actual APK camera sequences, event order, "
         "retained notices, empty promo campaigns and live hook lookup.");
    okay = true;
release:
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    ab_classic_start_level_close(fixture.start);
    ab_event_manager_close(events);
    ab_classic_camera_close(fixture.camera);
    ab_classic_level_close(level);
    for (size_t index = 0; index < 4; ++index) {
        ab_camera_sequence_free(fixture.sequences + index);
        ab_target_data_close(source + index);
    }
    ab_target_data_close(&ids);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    memset(key, 0, sizeof(key));
    return okay;
}

int main(int argc, char **argv) {
    return argc == 2 ? (check(argv[1]) ? 0 : 1) : 2;
}
