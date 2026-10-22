#ifndef AB_TEST_CLASSIC_GAME_INPUT_H
#define AB_TEST_CLASSIC_GAME_INPUT_H

#include "ab/classic_game_input.h"
#include "classic_game_menu_input.h"

typedef struct {
    AbClassicTouches *touches;
    AbClassicBirdSpecialty *specialty;
    AbUiFrameState frame_state;
    AbUiFrameNode notifications;
    AbDataTable empty;
    AbDataValue red_available;
    AbClassicGameInputState state;
    AbClassicGameInput input;
    float width;
} LifecycleInputBinding;

static bool lifecycle_input_binding(ShotTest *shot, AbClassicShots *shots,
                                    AbClassicSling *sling,
                                    AbClassicBehaviors *behaviors,
                                    AbCameraSystem *camera, AbGameTailState *tail,
                                    const bool *completed,
                                    LifecycleInputBinding *output, AbError *error) {
    *output = (LifecycleInputBinding){0};
    AbUiFrameFields fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("notifications")}};
    AbUiInitBoundaries boundaries = {0};
    CHECK(ab_ui_frame_init(&output->frame_state, &fields, &boundaries, error));
    output->notifications.state = &output->frame_state;
    output->width = ab_camera_system_view(camera).pixel_size.x;
    output->state.cursor = tail->cursor;
    output->state.hats = (AbDataValue){.kind = AB_DATA_TABLE, .table = &output->empty};
    output->state.disable_drag = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    output->state.skip_trigger_specialty = (AbDataValue){.kind = AB_DATA_NUMBER};
    CHECK(ab_classic_touches_open((AbClassicTouchesConfig){.level = shot->owner,
                                                           .behaviors = behaviors,
                                                           .camera = camera,
                                                           .capacity = 2,
                                                           .storage_bytes = 32768},
                                  &output->touches, error) &&
          ab_classic_bird_specialty_open(
              (AbClassicBirdSpecialtyConfig){.shots = shots,
                                             .behaviors = behaviors,
                                             .audio = shot->audio,
                                             .events = shot->events,
                                             .notifications = &output->notifications,
                                             .flying = &shot->flying,
                                             .red_available = &output->red_available,
                                             .notifications_capacity = 2,
                                             .storage_bytes = 32768},
              &output->specialty, error) &&
          ab_classic_game_input_initialize(
              (AbClassicGameInputConfig){.shots = shots,
                                         .sling = sling,
                                         .specialty = output->specialty,
                                         .touches = output->touches,
                                         .camera = camera,
                                         .tail = tail,
                                         .state = &output->state,
                                         .flying = &shot->flying,
                                         .level_completed = completed,
                                         .pressed = &output->empty,
                                         .held = &output->empty,
                                         .released = &output->empty,
                                         .screen_width = &output->width},
              &output->input, error));
    return true;
}

static void lifecycle_input_binding_close(LifecycleInputBinding *binding) {
    ab_classic_bird_specialty_close(binding->specialty);
    ab_classic_touches_close(binding->touches);
}

static bool lifecycle_touches(ShotTest *shot, AbClassicBehaviors *behaviors,
                              AbCameraSystem *camera, AbError *error) {
    AbClassicTouches *owner = NULL;
    CHECK(ab_classic_touches_open((AbClassicTouchesConfig){.level = shot->owner,
                                                           .behaviors = behaviors,
                                                           .camera = camera,
                                                           .capacity = 4,
                                                           .storage_bytes = 32768},
                                  &owner, error));
    AbClassicTouch touches[] = {
        {0, {10, 20}, {.kind = AB_DATA_NUMBER}},
        {5, {20, 30}, {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {-2, {30, 40}, {.kind = AB_DATA_STRING, .string = span("consumed")}}};
    AbDataValue consumed = {0};
    CHECK(ab_classic_touches_handle(owner, touches, 3, NAN, &consumed, error) &&
          consumed.kind == AB_DATA_NUMBER && !consumed.bits &&
          ab_classic_touches_view(owner).primary_present &&
          ab_classic_touches_view(owner).primary == 0 &&
          ab_classic_touches_view(owner).previous_count == 3);
    AbClassicTouch reordered[] = {touches[2], touches[1], touches[0]};
    CHECK(ab_classic_touches_handle(owner, reordered, 3, NAN, &consumed, error) &&
          ab_classic_touches_view(owner).primary == 0 &&
          consumed.kind == AB_DATA_NUMBER && !consumed.bits &&
          ab_classic_touches_handle(owner, touches + 1, 2, NAN, &consumed, error) &&
          ab_classic_touches_view(owner).primary == 5 &&
          consumed.kind == AB_DATA_BOOLEAN && consumed.bits);
    touches[0].position = (AbVec2){NAN, INFINITY};
    CHECK(ab_classic_touches_cancel(owner, touches, 3, error) &&
          ab_classic_touches_view(owner).primary == 5 &&
          ab_classic_touches_view(owner).previous_count == 2 &&
          ab_classic_touches_handle(owner, NULL, 0, NAN, &consumed, error) &&
          !ab_classic_touches_view(owner).primary_present &&
          !ab_classic_touches_view(owner).previous_count &&
          consumed.kind == AB_DATA_NIL);
    AbError late = {0};
    consumed = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(!ab_classic_touches_handle(owner, touches, 3, 0, &consumed, &late) &&
          strstr(late.message, "world transform") &&
          ab_classic_touches_view(owner).primary_present &&
          ab_classic_touches_view(owner).primary == 0 &&
          !ab_classic_touches_view(owner).previous_count && consumed.bits &&
          !ab_classic_touches_cancel(owner, NULL, 0, NULL));
    ab_classic_touches_close(owner);
    return true;
}

/* Explicit input probes over the real gameplay owners. Empty hats, key tables
 * and prepared menu snapshots are controlled inputs, not selected bootstrap. */
static bool lifecycle_input(ShotTest *shot, AbClassicShots *shots,
                            AbClassicSling *sling, AbClassicBehaviors *behaviors,
                            AbClassicBirdSpecialty *specialty, AbCameraSystem *camera,
                            AbError *error) {
    CHECK(lifecycle_touches(shot, behaviors, camera, error));
    AbClassicTouches *touches = NULL;
    CHECK(ab_classic_touches_open((AbClassicTouchesConfig){.level = shot->owner,
                                                           .behaviors = behaviors,
                                                           .camera = camera,
                                                           .capacity = 2,
                                                           .storage_bytes = 32768},
                                  &touches, error));
    AbDataTable hats = {0}, empty = {0};
    AbDataEntry press = {{.kind = AB_DATA_STRING, .string = span("LBUTTON")},
                         {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataEntry held = press;
    AbDataTable pressed = {&press, 1, 1, NULL}, hold = {&held, 1, 1, NULL};
    AbClassicGameInputState state = {.hats = {.kind = AB_DATA_TABLE, .table = &hats},
                                     .touch_count = {.kind = AB_DATA_NUMBER},
                                     .cursor = {10, 20}};
    AbClassicGameInput input = {0};
    AbGameTailState tail = {0};
    bool completed = false;
    float width = ab_camera_system_view(camera).pixel_size.x;
    CHECK(ab_classic_game_input_initialize(
        (AbClassicGameInputConfig){.shots = shots,
                                   .sling = sling,
                                   .specialty = specialty,
                                   .touches = touches,
                                   .camera = camera,
                                   .tail = &tail,
                                   .state = &state,
                                   .flying = &shot->flying,
                                   .level_completed = &completed,
                                   .pressed = &pressed,
                                   .held = &hold,
                                   .released = &empty,
                                   .screen_width = &width},
        &input, error));
    AbClassicTouch copied = {0, {10, 20}, {.kind = AB_DATA_NUMBER}};
    AbClassicTouch raw[] = {{3, {NAN, NAN}, {0}}, {9, {NAN, NAN}, {0}}};
    AbClassicGameTouchInput snapshot = {&copied, raw, 1, 2};
    ab_classic_shots_globals(shots)->specialty_available =
        (AbDataValue){.kind = AB_DATA_BOOLEAN};
    ab_classic_shots_state(shots)->specialty_available = false;
    CHECK(ab_classic_game_input_update(&input, snapshot, 0.005f,
                                       (AbDataValue){.kind = AB_DATA_NUMBER}, error) &&
          state.skip_input.kind == AB_DATA_NUMBER && !state.skip_input.bits &&
          ab_classic_sling_view(sling).mouse.drag_started &&
          ab_classic_sling_view(sling).mouse.in_game_pressed && tail.cursor.x == 10 &&
          tail.cursor.y == 20);
    /* This copy belongs to the tail, not to input or mouse-plan evaluation. */
    tail.old_cursor = tail.cursor;
    CHECK(ab_classic_game_input_finish_cursor(&input, error));
    press.value.bits = 0;
    state.cursor = (AbVec2){12, 23};
    CHECK(ab_classic_game_input_update(&input, snapshot, 0.005f,
                                       (AbDataValue){.kind = AB_DATA_NUMBER}, error));
    AbSlingMouseState mouse = ab_classic_sling_view(sling).mouse;
    CHECK(mouse.old_cursor.x == 10 && mouse.old_cursor.y == 20 &&
          mouse.samples[0].displacement.x == 2 && mouse.samples[0].displacement.y == 3);
    AbClassicGameInputConfig prepared_config = input.config;
    AbClassicTouch filtered[1], menu_copied[1];
    int32_t key[1];
    size_t index[1], array[1];
    AbIntegerOrderNode node[1];
    LifecycleGameMenuInput menu = {0};
    CHECK(lifecycle_menu_open(
        &input,
        (AbClassicMenuInputStorage){
            filtered, menu_copied, key, 1, {index, 1, array, 1, node, 1}},
        &menu, error));
    AbClassicTouch menu_touch = {77, {12, 23}, {.kind = AB_DATA_BOOLEAN}};
    menu.raw.touches = &menu_touch;
    menu.raw.touch_count = 1;
    state.skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_ui_menu_update(menu.manager, 0, 4, &menu.boundaries, error) &&
          ab_classic_menu_input_view(&menu.input).count == 1 &&
          ab_classic_game_input_update(&input,
                                       (AbClassicGameTouchInput){raw, raw, 2, 2}, 0,
                                       (AbDataValue){.kind = AB_DATA_NUMBER}, error) &&
          ab_classic_touches_view(touches).primary == 77 &&
          ab_classic_touches_view(touches).previous_count == 1 &&
          state.skip_input.kind == AB_DATA_BOOLEAN && !state.skip_input.bits &&
          menu_copied[0].id == 77 &&
          ab_classic_game_input_initialize(prepared_config, &input, error));
    ab_ui_menu_manager_close(menu.manager);
    state.disable_drag = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    state.skip_trigger_specialty = (AbDataValue){.kind = AB_DATA_NUMBER};
    CHECK(ab_classic_game_input_update(&input, snapshot, NAN,
                                       (AbDataValue){.kind = AB_DATA_NUMBER}, error) &&
          state.skip_input.kind == AB_DATA_BOOLEAN && state.skip_input.bits &&
          ab_classic_touches_view(touches).previous_count == 1 &&
          ab_classic_touches_view(touches).primary == 77);
    ab_classic_game_input_finish_draw(&input);
    CHECK(state.skip_input.kind == AB_DATA_BOOLEAN && !state.skip_input.bits &&
          state.skip_trigger_specialty.kind == AB_DATA_BOOLEAN &&
          !state.skip_trigger_specialty.bits);
    state.cheats_enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbError late = {0};
    CHECK(!ab_classic_game_input_update(&input, snapshot, 0,
                                        (AbDataValue){.kind = AB_DATA_NUMBER}, &late) &&
          strstr(late.message, "levelCompleteCheat") && input.failed &&
          state.skip_input.bits &&
          !ab_classic_game_input_update(&input, snapshot, 0, (AbDataValue){0}, NULL));
    ab_classic_touches_close(touches);
    return true;
}

#endif
