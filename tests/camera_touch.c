#include "ab/camera_touch.h"
#include "ab/menu_pointer.h"
#include "ab/touch_snapshot.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera touch failed at %d: %s\n", __LINE__, #condition);  \
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

static void drag_snapshot(const AbCameraDrag *state, uint32_t output[21]) {
    if (!state->initialized) {
        return;
    }
    output[0] = state->dragging ? 255u : 240u;
    const float values[] = {state->start.x,          state->start.y,
                            state->anchor.x,         state->anchor.y,
                            state->current_screen.x, state->current_screen.y,
                            state->old_screen.x,     state->old_screen.y};
    for (size_t field = 0; field < 8; ++field) {
        if (output[0] & (1u << field)) {
            output[1 + field] = bits(values[field]);
        }
    }
    output[9] = (uint32_t)state->count;
    for (size_t index = 0; index < state->count; ++index) {
        output[10 + index * 2] = bits(state->velocity[index].x);
        output[11 + index * 2] = bits(state->velocity[index].y);
    }
    output[20] = bits(5);
}

static void touch_snapshot(const AbCameraTouchHandler *handler,
                           const AbCameraTouchShared *shared,
                           const AbCameraTouchOutput *result, AbCameraSystem *system,
                           uint32_t conversions, uint32_t output[53]) {
    AbCameraSystemView view = ab_camera_system_view(system);
    output[0] = (uint32_t)handler->state;
    output[1] = bits(shared->tap_count);
    output[2] = bits(handler->timer);
    output[3] = (uint32_t)handler->first_id;
    output[4] = (uint32_t)handler->second_id;
    output[5] = handler->detect_tap;
    output[6] = handler->latest_tap_position_present;
    if (output[6]) {
        output[7] = bits(handler->latest_tap_position.x);
        output[8] = bits(handler->latest_tap_position.y);
    }
    output[9] = bits(handler->start_zoom);
    output[10] = bits(handler->start_distance);
    output[11] = bits(handler->previous_distance);
    output[12] = bits(handler->previous_diff);
    output[13] = bits(handler->zoom_velocity);
    drag_snapshot(&handler->drag, output + 14);
    output[35] = result->targets_present;
    output[36] = result->targets.values.present;
    if (output[36] & AB_CAMERA_VALUE_X) {
        output[37] = bits(result->targets.values.position.x);
    }
    if (output[36] & AB_CAMERA_VALUE_Y) {
        output[38] = bits(result->targets.values.position.y);
    }
    if (output[36] & AB_CAMERA_VALUE_ZOOM) {
        output[39] = bits(result->targets.values.zoom);
    }
    output[40] = bits(result->targets.movement_duration);
    output[41] = bits(result->targets.zoom_duration);
    output[42] = result->targets.movement_tween_present;
    output[43] = (uint32_t)result->targets.movement_tween;
    output[44] = result->targets.zoom_tween_present;
    output[45] = (uint32_t)result->targets.zoom_tween;
    output[46] = result->user_input;
    output[47] = (uint32_t)view.mover->movement_tween;
    output[48] = (uint32_t)view.mover->zoom_tween;
    output[49] = view.saved_toggle_position_present;
    if (output[49]) {
        output[50] = bits(view.saved_toggle_position.x);
        output[51] = bits(view.saved_toggle_position.y);
    }
    output[52] = conversions;
}

static uint32_t conversion_count(const AbCameraTouchHandler *handler,
                                 AbCameraSystem *system,
                                 const AbCameraTouchInput *input) {
    if (!ab_camera_system_view(system).input_enabled || input->bird_dragged ||
        (!input->specialty_available && !input->released_after_specialty)) {
        return 0;
    }
    bool first = false, second = false;
    for (size_t index = 0; index < input->count; ++index) {
        first |= input->touches[index].id == handler->first_id;
        second |= input->touches[index].id == handler->second_id;
    }
    return (handler->state == AB_CAMERA_TOUCH_DRAG && first) ||
           (handler->state == AB_CAMERA_TOUCH_ZOOM && first && second);
}

static bool pointer_consume(void *context, AbMenuPointerEvent event, AbVec2 cursor,
                            bool *consumed, AbError *error) {
    (void)cursor;
    (void)error;
    const uint32_t *mask = context;
    *consumed = (*mask & (1u << (unsigned)event)) != 0;
    return true;
}

static bool filter_touches(const uint32_t command[12], const AbCameraTouch *raw,
                           AbCameraTouch filtered[3], size_t *count, AbError *error) {
    AbMenuPointerRoute route = {pointer_consume, (void *)(command + 7)};
    AbMenuPointerInput input = {.events = command[5], .allow_input = command[6] != 0};
    AbMenuPointerResult result;
    int32_t keys[3];
    size_t first_copy[3], indices[3], array_slots[4];
    AbIntegerOrderNode nodes[4];
    AbTouchSnapshotWorkspace workspace = {
        keys, first_copy, 3, {indices, 3, array_slots, 4, nodes, 4}};
    return ab_menu_pointer_filter(&route, 1, &input, &result, error) &&
           ab_touch_snapshot(raw, command[3], result.copy_touches, &workspace, filtered,
                             3, count, error);
}

static bool compare_case(FILE *file, AbCameraSequence sources[2], size_t item,
                         bool filtered_input) {
    uint32_t setup[10];
    CHECK(words(file, setup, 10));
    AbCameraSnap snaps[2] = {{{number(setup[4]), number(setup[5])}, number(setup[6])},
                             {{number(setup[7]), number(setup[8])}, number(setup[9])}};
    AbCameraSystem *system = NULL;
    AbError error = {0};
    bool okay = false;
    Boundaries boundaries = {0};
    uint32_t publications = 0;
    AbCameraTouchHandler handler;
    AbCameraTouchShared shared;
    AbCameraTouchOutput result = {0};
    if (!ab_camera_system_open(snaps, 2, (AbVec2){number(setup[0]), number(setup[1])},
                               number(setup[2]), number(setup[3]), SIZE_MAX, &system,
                               &error) ||
        !ab_camera_touch_init(&handler, &shared)) {
        goto release_system;
    }
    for (size_t step = 0; step < 128; ++step) {
        uint32_t command[12], expected_system[46], expected_touch[53];
        uint32_t actual_system[46] = {0}, actual_touch[53] = {0};
        AbCameraTouch touches[3];
        if (!words(file, command, 12) || command[3] > 3) {
            goto release_system;
        }
        for (size_t index = 0; index < command[3]; ++index) {
            uint32_t record[3];
            if (!words(file, record, 3)) {
                goto release_system;
            }
            float id = number(record[0]);
            if (!isfinite(id) || id < -16777216 || id > 16777216 || id != truncf(id)) {
                goto release_system;
            }
            touches[index] =
                (AbCameraTouch){(int32_t)id, {number(record[1]), number(record[2])}};
        }
        if (!words(file, expected_system, 46) || !words(file, expected_touch, 53)) {
            goto release_system;
        }
        if (command[4] && !ab_camera_system_input_enabled(system, command[4] == 2)) {
            goto release_system;
        }
        AbCameraTouchInput input = {.touches = touches,
                                    .count = command[3],
                                    .bird_dragged = (command[2] & 64) != 0,
                                    .specialty_available = (command[2] & 4) != 0,
                                    .released_after_specialty = (command[2] & 16) != 0,
                                    .ingame = (command[2] & 32) != 0,
                                    .suppress_zoom_snap = (command[2] & 256) != 0};
        AbCameraTouch filtered[3];
        if (filtered_input) {
            size_t filtered_count;
            if (!filter_touches(command, touches, filtered, &filtered_count, &error)) {
                goto release_system;
            }
            input.touches = filtered;
            input.count = filtered_count;
        }
        uint32_t conversions = 0;
        bool applied;
        if (command[0] < 2) {
            applied =
                ab_camera_system_start(system, sources + command[0],
                                       command[11] ? done : NULL, &boundaries, &error);
        } else {
            conversions = conversion_count(&handler, system, &input);
            float seconds = number(command[1]);
            applied = ab_camera_touch_update(&handler, &shared, system, seconds, &input,
                                             &result, &error);
            AbCameraSystemInput resolved = {
                .targets = result.targets,
                .targets_present = result.targets_present,
                .user_input = result.user_input,
                .moving_objects = (command[2] & 2) != 0,
                .specialty_available = input.specialty_available,
                .shake = {number(command[9]), number(command[10])}};
            applied =
                applied && ab_camera_system_update(system, seconds, &resolved, &error);
            ++publications;
        }
        if (!applied) {
            fprintf(stderr, "Touch case %zu step %zu: %s\n", item, step, error.message);
            goto release_system;
        }
        snapshot(system, publications, &boundaries, actual_system);
        touch_snapshot(&handler, &shared, &result, system, conversions, actual_touch);
        for (size_t field = 0; field < 99; ++field) {
            uint32_t actual =
                field < 46 ? actual_system[field] : actual_touch[field - 46];
            uint32_t expected =
                field < 46 ? expected_system[field] : expected_touch[field - 46];
            if (actual != expected) {
                fprintf(stderr,
                        "Touch case %zu step %zu %s field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        item, step, field < 46 ? "camera" : "handler",
                        field < 46 ? field : field - 46, actual, expected);
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
          (strcmp(header, "AB_ANDROID_CAMERA_TOUCH_V1 64 128\n") == 0 ||
           strcmp(header, "AB_ANDROID_CAMERA_TOUCH_FILTERED_V1 64 128\n") == 0));
    bool filtered = strcmp(header, "AB_ANDROID_CAMERA_TOUCH_FILTERED_V1 64 128\n") == 0;
    for (uint32_t flags = 0; flags < 8; ++flags) {
        uint32_t gate[2];
        CHECK(words(file, gate, 2) && gate[0] == flags &&
              gate[1] == (uint32_t)(!(flags & 1) && (flags & 6)));
    }
    for (size_t item = 0; item < 64; ++item) {
        CHECK(compare_case(file, sources, item, filtered));
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, filtered ? "AB camera touch filtered reference complete\n"
                                  : "AB camera touch reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 8192 original touch commands match handler state, pre-limit targets, "
         "shared taps, builtin tweens and composed camera state.");
    if (filtered) {
        puts("Original menu pointer filtering and both fresh touch-copy orders match.");
    }
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

static bool snapshot_checks(void) {
    AbCameraTouch raw[] = {{0, {1, 2}}, {1, {3, 4}}};
    AbCameraTouch output[2] = {{99, {7, 8}}, {98, {9, 10}}};
    int32_t keys[2];
    size_t first_copy[2], indices[2], slots[2], count = 77;
    AbIntegerOrderNode nodes[2];
    AbTouchSnapshotWorkspace workspace = {
        keys, first_copy, 2, {indices, 2, slots, 2, nodes, 2}};
    CHECK(ab_touch_snapshot(raw, 2, true, &workspace, output, 2, &count, NULL) &&
          count == 2 && output[0].id == 1 && output[1].id == 0 &&
          output[0].position.x == 3 && output[1].position.y == 2);
    CHECK(ab_touch_snapshot(NULL, SIZE_MAX, false, NULL, NULL, 0, &count, NULL) &&
          count == 0 && ab_touch_snapshot(NULL, 0, true, NULL, NULL, 0, &count, NULL));
    count = 77;
    raw[1].id = raw[0].id;
    CHECK(!ab_touch_snapshot(raw, 2, true, &workspace, output, 2, &count, NULL) &&
          count == 77 && output[0].id == 1 && output[1].id == 0 &&
          !ab_touch_snapshot(raw, 2, true, &workspace, output, 1, &count, NULL) &&
          !ab_touch_snapshot(raw, 2, true, NULL, output, 2, &count, NULL) &&
          !ab_touch_snapshot(raw, 2, true, &workspace, output, 2, NULL, NULL));
    puts("Filtered touch snapshots preserve copy order, numeric payload and failure "
         "output.");
    return true;
}

static bool edge_checks(void) {
    AbCameraSnap snaps[] = {{{-30, -300}, 0.5f}, {{600, -300}, 0.5f}};
    AbCameraSystem *camera = NULL;
    AbCameraTouchHandler handler;
    AbCameraTouchShared shared;
    AbCameraTouchOutput result;
    AbCameraTouch touch = {3, {120, 130}};
    AbCameraTouchInput input = {.touches = &touch,
                                .count = 1,
                                .ingame = true,
                                .released_after_specialty = true};
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){480, 320}, 200, 200, SIZE_MAX,
                                &camera, NULL) &&
          ab_camera_touch_init(&handler, &shared) &&
          ab_camera_system_update(camera, 0, &(AbCameraSystemInput){0}, NULL));
    for (size_t tap = 0; tap < 2; ++tap) {
        input.count = 1;
        CHECK(ab_camera_touch_update(&handler, &shared, camera, 0.015625f, &input,
                                     &result, NULL) &&
              !result.user_input && !result.targets_present &&
              handler.state == AB_CAMERA_TOUCH_DRAG);
        input.count = 0;
        CHECK(ab_camera_touch_update(&handler, &shared, camera, 0.015625f, &input,
                                     &result, NULL) &&
              result.user_input && result.targets_present &&
              handler.state == AB_CAMERA_TOUCH_IDLE &&
              shared.tap_count == (float)(tap + 1) &&
              result.targets.movement_tween == AB_CAMERA_TWEEN_CUBIC_OUT);
    }
    CHECK(ab_camera_system_view(camera).automatic_present &&
          ab_camera_system_view(camera).saved_toggle_position_present);
    input.bird_dragged = true;
    CHECK(ab_camera_touch_update(&handler, &shared, camera, 0.1f, &input, &result,
                                 NULL) &&
          !result.user_input && !result.targets_present && shared.tap_count == 0 &&
          handler.timer == 0);
    AbCameraTouch pair[] = {{3, {150, 160}}, {10, {150, 160}}};
    input = (AbCameraTouchInput){
        .touches = pair, .count = 2, .ingame = true, .released_after_specialty = true};
    CHECK(ab_camera_touch_update(&handler, &shared, camera, 0, &input, &result, NULL) &&
          handler.state == AB_CAMERA_TOUCH_ZOOM && handler.start_distance == 0);
    CHECK(ab_camera_touch_update(&handler, &shared, camera, 0, &input, &result, NULL) &&
          result.targets_present &&
          bits(result.targets.values.zoom) == UINT32_C(0x7fc00000) &&
          result.targets.zoom_tween_present &&
          result.targets.zoom_tween == AB_CAMERA_TWEEN_END);
    CHECK(ab_camera_system_update(camera, 0,
                                  &(AbCameraSystemInput){.targets = result.targets,
                                                         .targets_present = true,
                                                         .user_input = true},
                                  NULL));
    AbCameraTouchHandler saved = handler;
    AbCameraTouchOutput saved_output = result;
    pair[1].id = pair[0].id;
    CHECK(
        !ab_camera_touch_update(&handler, &shared, camera, 0, &input, &result, NULL) &&
        memcmp(&handler, &saved, sizeof(saved)) == 0 &&
        memcmp(&result, &saved_output, sizeof(result)) == 0 &&
        !ab_camera_touch_init(NULL, &shared) && !ab_camera_touch_init(&handler, NULL) &&
        !ab_camera_system_goto_snap(camera, 2, true, true, NULL, NULL));
    CHECK(ab_camera_system_input_enabled(camera, false));
    pair[1].id = 10;
    CHECK(ab_camera_touch_update(&handler, &shared, camera, 0, &input, &result, NULL) &&
          handler.state == AB_CAMERA_TOUCH_IDLE && !result.targets_present &&
          !result.user_input);
    ab_camera_system_close(camera);
    puts("Camera touch double taps, cancellation, coincident pinch clamping and "
         "invalid snapshot preservation pass.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() && snapshot_checks() ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--apk") == 0) {
        return apk_reference(argv[2], argv[3]) ? 0 : 1;
    }
    return 2;
}
