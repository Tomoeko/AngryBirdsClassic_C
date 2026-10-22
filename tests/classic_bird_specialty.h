#ifndef AB_TEST_CLASSIC_BIRD_SPECIALTY_H
#define AB_TEST_CLASSIC_BIRD_SPECIALTY_H

#include "ab/classic_bird_specialty.h"
#include "classic_game_input.h"

typedef struct {
    AbClassicBirdSpecialty *owner;
    AbClassicShots *shots;
    AbAudioMixer *mixer;
    const AbDataTable *physical, *events[2];
    size_t count;
} LifecycleSpecialtyObserver;

static bool lifecycle_specialty_event(void *context, AbDataTable *event,
                                      AbError *error) {
    LifecycleSpecialtyObserver *test = context;
    (void)error;
    AbClassicGameGlobals *globals = ab_classic_shots_globals(test->shots);
    CHECK(test->count < 2 &&
          damage_name_equal(
              field(event, "id"),
              (AbDataValue){.kind = AB_DATA_STRING,
                            .string = span("EID_BIRD_SPECIALTY_TRIGGERED")}) &&
          globals->specialty_available.kind == AB_DATA_BOOLEAN &&
          !globals->specialty_available.bits &&
          !ab_classic_shots_state(test->shots)->specialty_available &&
          globals->bird_specialty.kind == AB_DATA_NIL &&
          field(test->physical, "specialtyTriggered").bits &&
          field(test->physical, "inBubble").kind == AB_DATA_BOOLEAN &&
          !field(test->physical, "inBubble").bits &&
          ab_audio_mixer_view(test->mixer).pending_count == test->count + 1 &&
          !ab_classic_bird_specialty_trigger(test->owner, (AbDataValue){0}, NAN, NULL));
    if (test->count)
        CHECK(test->events[0] != event);
    test->events[test->count++] = event;
    return true;
}

static bool lifecycle_specialty(ShotTest *shot, AbClassicShots *shots,
                                AbClassicBehaviors *behaviors, AbClassicSling *sling,
                                AbCameraSystem *camera, AbError *error) {
    AbClassicLevelView level = shot->level;
    size_t object = SIZE_MAX;
    for (size_t index = 0; index < level.bird_count; ++index) {
        size_t candidate = level.bird_order[index];
        if (!level.removed_objects[candidate] &&
            damage_name_equal(
                field(&level.properties->objects[candidate].physical, "definition"),
                (AbDataValue){.kind = AB_DATA_STRING, .string = span("RedBird")})) {
            object = candidate;
            break;
        }
    }
    CHECK(object != SIZE_MAX);
    AbUiFrameState parent_state = {0}, child_state = {0};
    AbUiFrameFields parent_fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("notifications")}};
    AbUiFrameFields child_fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("redBirdFingerTutorial")}};
    AbUiInitBoundaries boundaries = {0};
    CHECK(ab_ui_frame_init(&parent_state, &parent_fields, &boundaries, error) &&
          ab_ui_frame_init(&child_state, &child_fields, &boundaries, error));
    AbUiFrameNode *children[1] = {0};
    AbUiFrameNode notifications = {
        .state = &parent_state, .children = children, .capacity = 1};
    AbUiFrameNode child = {.state = &child_state};
    AbDataValue available = {.kind = AB_DATA_BOOLEAN};
    AbClassicBirdSpecialty *owner = NULL;
    CHECK(ab_classic_bird_specialty_open(
        (AbClassicBirdSpecialtyConfig){.shots = shots,
                                       .behaviors = behaviors,
                                       .audio = shot->audio,
                                       .events = shot->events,
                                       .notifications = &notifications,
                                       .flying = &shot->flying,
                                       .red_available = &available,
                                       .notifications_capacity = 2,
                                       .storage_bytes = 32768},
        &owner, error));
    /* This new controlled phase starts with an explicitly empty mixer. Native
     * output/device timing is not part of these module-boundary calls. */
    int16_t scratch = 0;
    CHECK(ab_audio_mixer_all_action(shot->mixer, AB_AUDIO_STOP, error) &&
          ab_audio_mixer_mix(shot->mixer, 0, &scratch, 1, error) &&
          ab_audio_mixer_mix(shot->mixer, 0, &scratch, 1, error) &&
          !ab_audio_mixer_view(shot->mixer).active_count &&
          !ab_audio_mixer_view(shot->mixer).pending_count &&
          ab_audio_mixer_view(shot->mixer).started);
    AbDataValue saved_flying = shot->flying;
    shot->flying = (AbDataValue){0};
    AbClassicGameGlobals *globals = ab_classic_shots_globals(shots);
    globals->specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    ab_classic_shots_state(shots)->specialty_available = true;
    globals->bird_specialty =
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("retainedSpecialty")};
    CHECK(ab_classic_bird_specialty_trigger_flying(owner, NAN, error) &&
          !globals->specialty_available.bits &&
          !ab_classic_shots_state(shots)->specialty_available &&
          globals->bird_specialty.kind == AB_DATA_STRING &&
          !ab_audio_mixer_view(shot->mixer).pending_count);
    AbDataTable *physical = &level.properties->objects[object].physical;
    LifecycleSpecialtyObserver observer = {owner, shots, shot->mixer, physical, {0}, 0};
    AbEventListener listener = {span("LifecycleSpecialty"), &observer,
                                lifecycle_specialty_event};
    AbDataValue event_id = {.kind = AB_DATA_STRING,
                            .string = span("EID_BIRD_SPECIALTY_TRIGGERED")};
    CHECK(ab_event_manager_add(shot->events, event_id, &listener, error));
    shot->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical};
    CHECK(ab_level_property_set(level.properties, object, span("inBubble"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                error) &&
          ab_classic_bird_specialty_trigger_flying(owner, NAN, error) &&
          observer.count == 1 &&
          ab_classic_bird_specialty_trigger(owner, shot->flying, NAN, error) &&
          observer.count == 2 &&
          lifecycle_input(shot, shots, sling, behaviors, owner, camera, error) &&
          ab_ui_frame_add_child(&notifications, &child, 0, false, error));
    AbError missing = {0};
    CHECK(!ab_classic_bird_specialty_trigger_flying(owner, 0, &missing) &&
          strstr(missing.message, "Active red tutorial/forced targeting") &&
          observer.count == 2 && ab_audio_mixer_view(shot->mixer).pending_count == 2 &&
          !ab_classic_bird_specialty_trigger_flying(owner, 0, NULL) &&
          damage_name_equal(field(observer.events[0], "id"), event_id) &&
          ab_event_manager_remove(shot->events, event_id, &listener, error) &&
          ab_ui_frame_remove_child(&notifications, &child, error));
    shot->flying = saved_flying;
    ab_classic_bird_specialty_close(owner);
    return true;
}

#endif
