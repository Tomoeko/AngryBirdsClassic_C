#ifndef AB_TEST_CLASSIC_BIRDS_H
#define AB_TEST_CLASSIC_BIRDS_H

#include "ab/classic_birds.h"
#include "ab/classic_controller.h"
#include "ab/classic_character_services.h"
#include "ab/classic_game_effects.h"
#include "ab/classic_game_prelude.h"
#include "ab/classic_level_finish.h"
#include "ab/classic_level_ending.h"
#include "ab/classic_game_progress.h"
#include "ab/target_data.h"
#include "ab/camera_bootstrap.h"
#include "classic_game_start.h"
#include "classic_game_powerups.h"
#include "classic_game_level_particles.h"
#include "classic_bird_specialty.h"
#include "classic_game_update.h"
#include "classic_sling_draw.h"
#include "classic_game_draw.h"

static bool lifecycle_hook(void *context, AbDataValue name, size_t object,
                           AbError *error) {
    BirdCollisionTest *test = context;
    CHECK(object == test->first &&
          damage_name_equal(name, (AbDataValue){.kind = AB_DATA_STRING,
                                                .string = span("redBirdRemoved")}));
    return ab_classic_red_bird_removed(false, error);
}

static bool lifecycle_volume(void *context, AbDataValue receiver, size_t object,
                             AbDataValue *output, AbError *error) {
    (void)error;
    BirdCollisionTest *test = context;
    CHECK(object == test->first && receiver.kind == AB_DATA_TABLE);
    *output = (AbDataValue){.kind = AB_DATA_NUMBER,
                            .bits = bits(ab_level_effect_base_bird_volume())};
    return true;
}

typedef struct {
    const AbCameraSequence *sequences;
    AbSpan logs[12];
    size_t count;
} LifecycleCameraResources;

static bool lifecycle_sequence(void *context, const char *name,
                               const AbCameraSequence **output, AbError *error) {
    LifecycleCameraResources *resources = context;
    if (!strcmp(name, "FollowShotBirdEndCameraSequence"))
        *output = resources->sequences;
    else if (!strcmp(name, "FollowShotBirdCameraSequence"))
        *output = resources->sequences + 1;
    else if (!strcmp(name, "LevelEndSequence"))
        *output = resources->sequences + 2;
    else
        return ab_fail(error, "Unknown lifecycle camera sequence");
    return true;
}

static bool lifecycle_camera_log(void *context, AbSpan message, AbError *error) {
    LifecycleCameraResources *resources = context;
    if (resources->count == sizeof(resources->logs) / sizeof(*resources->logs))
        return ab_fail(error, "Lifecycle camera log storage exhausted");
    resources->logs[resources->count++] = message;
    return true;
}

typedef struct {
    AbDrawList *list;
    size_t draws;
    bool fail_draw;
    LifecycleInputBinding *input;
    const unsigned *phase;
    AbClassicGameDraw *game_draw;
} LifecycleRender;

/* Actual ordinary drawGame producer at the controller's draw boundary.
 * Root/MenuManager/HUD and a complete native GPU frame remain separate. */
static bool lifecycle_render(void *context, AbGameTailCall *call, AbError *error) {
    LifecycleRender *render = context;
    CHECK(call->operation == AB_GAME_TAIL_DRAW);
    CHECK(!render->phase || *render->phase == 3);
    AbClassicGameInput *input = &render->input->input;
    AbVec2 cursor = input->config.tail->old_cursor;
    AbSlingMouseState mouse = ab_classic_sling_view(input->config.sling).mouse;
    CHECK(cursor.x == mouse.old_cursor.x && cursor.y == mouse.old_cursor.y &&
          input->config.state->skip_input.kind == AB_DATA_BOOLEAN &&
          input->config.state->skip_input.bits &&
          input->config.state->skip_trigger_specialty.kind == AB_DATA_NUMBER);
    ++render->draws;
    if (render->fail_draw)
        return ab_fail(error, "Intentional controller draw failure");
    render->list->count = 0;
    return ab_classic_game_draw(render->game_draw, 480, 320, call->time, render->list,
                                error);
}

typedef struct {
    AbClassicLevelFinish *owner;
    AbClassicLevelFinishState *state;
    AbClassicShots *shots;
    const AbDataTable *event;
    size_t notifications;
} LifecycleFinishObserver;

static bool lifecycle_finish_event(void *context, AbDataTable *event, AbError *error) {
    LifecycleFinishObserver *observer = context;
    CHECK(damage_name_equal(field(event, "id"),
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = span("EID_LEVEL_COMPLETE_INIT")}));
    CHECK(!observer->state->complete.completed &&
          ab_classic_shots_state(observer->shots)->dragged &&
          !ab_classic_level_finish_complete(observer->owner, NULL));
    (void)error;
    observer->event = event;
    ++observer->notifications;
    return true;
}

typedef struct {
    const AbDataTable *events[4];
    size_t count;
} LifecycleEndingObserver;

static bool lifecycle_ended_event(void *context, AbDataTable *event, AbError *error) {
    LifecycleEndingObserver *observer = context;
    (void)error;
    CHECK(observer->count < 4 &&
          damage_name_equal(field(event, "id"),
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = span("EID_LEVEL_ENDED")}) &&
          damage_name_equal(
              field(event, "level"),
              (AbDataValue){.kind = AB_DATA_STRING, .string = span("Level1")}) &&
          field(event, "score").bits == bits(15640) &&
          field(event, "levelComplete").kind == AB_DATA_BOOLEAN &&
          !!field(event, "levelComplete").bits == (observer->count < 3));
    for (size_t index = 0; index < observer->count; ++index)
        CHECK(observer->events[index] != event);
    observer->events[observer->count++] = event;
    return true;
}

typedef struct {
    AbClassicGameProgress *progress;
    AbClassicLevelFinishState *finish;
    AbDataValue *triggered;
    const AbDataTable *events[2];
    size_t count;
} LifecycleProgressObserver;

static bool lifecycle_will_end_event(void *context, AbDataTable *event,
                                     AbError *error) {
    LifecycleProgressObserver *observer = context;
    CHECK(observer->count < 2 &&
          damage_name_equal(field(event, "id"),
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = span("EID_LEVEL_WILL_END")}));
    CHECK(observer->triggered->kind == AB_DATA_BOOLEAN && observer->triggered->bits &&
          observer->finish->complete.completed &&
          observer->finish->complete.ending_timer == (observer->count ? 0.5f : 1) &&
          !ab_classic_game_progress_update(observer->progress, 0, NULL));
    if (observer->count)
        CHECK(observer->events[0] != event);
    observer->events[observer->count++] = event;
    return observer->count == 1 || ab_fail(error, "Intentional will-end failure");
}

static bool lifecycle_progress(ShotTest *shot, AbClassicLevelFinish *finish,
                               AbClassicLevelFinishState *finish_state,
                               AbClassicLevelEnding *ending,
                               AbClassicLevelEndingState *ending_state,
                               AbClassicCharacterServices *conditions,
                               AbDataValue *prevent, AbError *error) {
    AbClassicGameProgress *progress = NULL;
    AbDataValue key_c = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbDataValue key_gaming_a = {0}, triggered = {0};
    CHECK(ab_classic_game_progress_open(
        (AbClassicGameProgressConfig){.finish = finish,
                                      .ending = ending,
                                      .finish_state = finish_state,
                                      .conditions = conditions,
                                      .events = shot->events,
                                      .cheats_enabled = &ending_state->cheats_enabled,
                                      .key_c = &key_c,
                                      .key_gaming_a = &key_gaming_a,
                                      .quad_click = &ending_state->quad_click,
                                      .level_will_end_triggered = &triggered,
                                      .notifications = 2,
                                      .storage_bytes = 32768},
        &progress, error));
    LifecycleProgressObserver observer = {progress, finish_state, &triggered, {0}, 0};
    AbEventListener listener = {span("LifecycleProgress"), &observer,
                                lifecycle_will_end_event};
    AbDataValue event_id = {.kind = AB_DATA_STRING,
                            .string = span("EID_LEVEL_WILL_END")};
    CHECK(ab_event_manager_add(shot->events, event_id, &listener, error));
    CHECK(!finish_state->complete.completed &&
          finish_state->complete.ending_timer == 0 && prevent->bits &&
          ab_classic_game_progress_update(progress, NAN, error) && !observer.count &&
          triggered.kind == AB_DATA_NIL);
    /* Numeric zero is truthy for the cheat gate. Prevention stops the actual
     * win/loss queries, but the original cheat fallback still completes. */
    ending_state->cheats_enabled = (AbDataValue){.kind = AB_DATA_NUMBER};
    size_t audio = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(ab_classic_game_progress_update(progress, 0, error) && observer.count == 1 &&
          finish_state->complete.completed &&
          finish_state->complete.bonus_timer == 3.5f &&
          ab_audio_mixer_view(shot->mixer).pending_count > audio &&
          ab_classic_game_progress_update(progress, 0.125f, error) &&
          finish_state->complete.bonus_timer == 3.375f && observer.count == 1);
    *prevent = (AbDataValue){0};
    key_c = (AbDataValue){0};
    ending_state->cheats_enabled = (AbDataValue){0};
    finish_state->complete.completed = false;
    finish_state->complete.ending_timer = 0;
    /* Cleared goals plus timeout make both queries true. Failure initialization
     * runs after completion, and a prior trigger suppresses its announcement. */
    CHECK(ab_classic_game_progress_update(progress, 0, error) && observer.count == 1 &&
          finish_state->complete.completed &&
          finish_state->complete.ending_timer == 0.5f &&
          finish_state->failed_timer == 2.625f);
    triggered = (AbDataValue){0};
    finish_state->complete.completed = false;
    finish_state->complete.ending_timer = 0;
    AbError late = {0};
    CHECK(!ab_classic_game_progress_update(progress, 0, &late) &&
          strstr(late.message, "Intentional will-end failure") && observer.count == 2 &&
          triggered.bits && finish_state->complete.completed &&
          finish_state->complete.ending_timer == 0.5f &&
          !ab_classic_game_progress_update(progress, 0, NULL) && observer.count == 2 &&
          damage_name_equal(field(observer.events[0], "id"), event_id) &&
          ab_event_manager_remove(shot->events, event_id, &listener, error));
    ab_classic_game_progress_close(progress);
    return true;
}

static bool
lifecycle_ending(ShotTest *shot, AbClassicShots *shots, AbClassicSling *sling,
                 AbClassicScores *scores, AbFloatingScores *floating,
                 AbClassicLevelFinish *finish_owner, AbClassicLevelFinishState *finish,
                 const AbClassicCharacterServices *conditions, AbError *error) {
    AbClassicLevelEnding *ending = NULL;
    AbClassicLevelEndingState state = {
        .skip_ending = {.kind = AB_DATA_BOOLEAN, .bits = 1},
        .eagle_darkness = {.kind = AB_DATA_BOOLEAN, .bits = 1},
        .level_name = {.kind = AB_DATA_STRING, .string = span("Level1")},
        .score_increment = 10000,
        .floating_scale = 1,
        .default_delay = 1};
    float entries[4] = {0};
    AbClassicBirdBonusScores bonus = {entries, 0, 4};
    AbDataValue eagle = {0}, eagle_timer = {0}, prevent = {0}, moving = {0};
    AbClassicCharacterServices live = *conditions;
    live.config.do_not_wait_for_moving_objects = &state.do_not_wait;
    live.config.eagle_launched = &eagle;
    live.config.eagle_timer = &eagle_timer;
    live.config.prevent_ending = &prevent;
    live.config.moving_objects = &moving;
    finish->complete.ending_timer = 1;
    CHECK(ab_classic_level_ending_open(
        (AbClassicLevelEndingConfig){.shots = shots,
                                     .sling = sling,
                                     .scores = scores,
                                     .floating = floating,
                                     .audio = shot->audio,
                                     .events = shot->events,
                                     .conditions = &live,
                                     .finish = finish,
                                     .state = &state,
                                     .bonus = &bonus,
                                     .eagle_launched = &eagle,
                                     .bird_sound = conditions->config.bird_sound,
                                     .notifications = 4,
                                     .storage_bytes = 65536},
        &ending, error));
    LifecycleEndingObserver observer = {0};
    AbEventListener listener = {span("LifecycleEnding"), &observer,
                                lifecycle_ended_event};
    AbDataValue event_id = {.kind = AB_DATA_STRING, .string = span("EID_LEVEL_ENDED")};
    CHECK(ab_event_manager_add(shot->events, event_id, &listener, error));
    float *bird_score = ab_classic_scores_view(scores).birds;
    size_t floating_count = ab_floating_scores_count(floating);
    size_t audio = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(*bird_score == 0 && ab_classic_level_ending_update(ending, NAN, error) &&
          finish->complete.bonus_timer == 3.5f && state.eagle_darkness.bits &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio);
    state.skip_ending = (AbDataValue){0};
    CHECK(ab_classic_level_ending_update(ending, 3.5f, error) &&
          finish->complete.bonus_timer == 0 && finish->complete.ending_timer == 1 &&
          state.eagle_darkness.kind == AB_DATA_NIL && !bonus.count &&
          ab_floating_scores_count(floating) == floating_count &&
          ab_classic_level_ending_update(ending, 0.125f, error) &&
          finish->complete.remaining_bird_index == 2 &&
          finish->complete.bonus_timer == 0.5f && finish->complete.ending_timer == 1 &&
          *bird_score == 10000 && bonus.count == 1 && entries[0] == 10000 &&
          shot->globals.score.bits == bits(15640) &&
          ab_floating_scores_count(floating) == floating_count + 1 &&
          ab_audio_mixer_view(shot->mixer).pending_count > audio);
    AbBirdCollisionFloatingScore *record =
        ab_floating_scores_at(floating, floating_count);
    CHECK(record && record->score == 10000 && record->time == 0 &&
          record->lifetime == 0.9f && record->max_scale == 1 && record->xs == 0 &&
          !record->text_present && record->sprite.kind == AB_DATA_STRING);
    eagle = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    audio = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(ab_classic_level_ending_update(ending, 0.625f, error));
    CHECK(finish->complete.remaining_bird_index == 3);
    CHECK(finish->complete.bonus_timer == 0.5f && *bird_score == 10000 &&
          bonus.count == 1);
    CHECK(ab_floating_scores_count(floating) == floating_count + 2);
    CHECK(ab_audio_mixer_view(shot->mixer).pending_count > audio);
    eagle = (AbDataValue){0};
    CHECK(ab_classic_level_ending_update(ending, 0.625f, error) &&
          state.before_script_called && finish->complete.ending_timer == 0.375f &&
          ab_classic_level_ending_update(ending, 0.375f, error) &&
          finish->complete.ending_timer == 0 && shot->globals.level_timeout.bits &&
          ab_classic_level_ending_update(ending, 0, error));
    finish->complete.completed = false;
    CHECK(ab_classic_level_ending_update(ending, 0, error));
    prevent = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_level_ending_update(ending, 0, error) &&
          ab_event_manager_tick(shot->events, 0, error) && observer.count == 4 &&
          shot->globals.score.bits == bits(15640) &&
          ab_event_manager_remove(shot->events, event_id, &listener, error));
    CHECK(lifecycle_progress(shot, finish_owner, finish, ending, &state, &live,
                             &prevent, error));
    ab_classic_level_ending_close(ending);
    return true;
}

static bool lifecycle_finish(ShotTest *shot, AbClassicShots *shots,
                             AbClassicBehaviors *behaviors, AbClassicCamera *camera,
                             const AbCameraSequence *end_sequence,
                             AbClassicScores *scores, AbFloatingScores *floating,
                             const AbClassicCharacterServices *conditions,
                             AbError *error) {
    AbClassicSling *sling = NULL;
    AbClassicLevelFinish *finish = NULL;
    AbClassicLevelFinishState state = {
        .complete = {.bonus_timer = -9, .ending_timer = -9}};
    AbDataValue clear_sound = {.kind = AB_DATA_STRING,
                               .string = span("level_clear_military")};
    AbDataValue failed_sound = {.kind = AB_DATA_STRING,
                                .string = span("level_failed_piglets")};
    CHECK(ab_classic_sling_open(
        (AbClassicSlingConfig){.level = shot->owner,
                               .shots = shots,
                               .behaviors = behaviors,
                               .services = shot->bindings,
                               .events = shot->events,
                               .audio = shot->audio,
                               .camera = ab_classic_camera_system(camera),
                               .start = {.level_start = shot->rest, .band = shot->rest},
                               .physics_scale = shot->physics_scale,
                               .default_force = shot->default_force,
                               .current_bird_index = 1,
                               .storage_bytes = 65536},
        &sling, error));
    size_t object = SIZE_MAX;
    CHECK(ab_classic_sling_next_bird(sling, 1, &object, error) && object != SIZE_MAX);
    AbShotState *shared = ab_classic_shots_state(shots);
    CHECK(!shared->current_bird_name && !shared->dragged && shared->birds_shot == 0);
    shared->dragged = ab_classic_shots_bird(shots, object);
    CHECK(ab_classic_level_finish_open(
        (AbClassicLevelFinishConfig){.shots = shots,
                                     .sling = sling,
                                     .camera = camera,
                                     .audio = shot->audio,
                                     .events = shot->events,
                                     .state = &state,
                                     .clear_sound = &clear_sound,
                                     .failed_sound = &failed_sound,
                                     .notifications = 4,
                                     .storage_bytes = 65536},
        &finish, error));
    LifecycleFinishObserver observer = {finish, &state, shots, NULL, 0};
    AbEventListener listener = {span("LifecycleFinish"), &observer,
                                lifecycle_finish_event};
    AbDataValue event_id = {.kind = AB_DATA_STRING,
                            .string = span("EID_LEVEL_COMPLETE_INIT")};
    CHECK(ab_event_manager_add(shot->events, event_id, &listener, error));
    size_t audio = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(ab_classic_level_finish_complete(finish, error) &&
          observer.notifications == 1 && observer.event && state.complete.completed &&
          state.complete.completing && state.complete.remaining_bird_index == 1 &&
          state.complete.bonus_timer == 3.5f && state.complete.ending_timer == 1 &&
          !shared->dragged &&
          ab_classic_shots_bird(shots, object)->position.x == shot->rest.x &&
          ab_classic_shots_bird(shots, object)->position.y == shot->rest.y &&
          ab_classic_camera_view(camera).loaded_sequence &&
          ab_classic_camera_view(camera).loaded_sequence->count ==
              end_sequence->count &&
          ab_classic_camera_view(camera).loaded_sequence->stages[0].properties ==
              end_sequence->stages[0].properties &&
          ab_audio_mixer_view(shot->mixer).pending_count > audio);
    audio = ab_audio_mixer_view(shot->mixer).pending_count;
    state.complete.completing = false;
    CHECK(ab_classic_level_finish_failed(finish, 1.5f, error) &&
          state.failed_timer == 1.5f && !state.complete.completing &&
          state.complete.ending_timer == 1 &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio &&
          ab_classic_level_finish_failed(finish, 0.125f, error) &&
          state.failed_timer == 1.625f && state.complete.completing &&
          state.complete.ending_timer == 0.5f &&
          ab_audio_mixer_view(shot->mixer).pending_count > audio);
    audio = ab_audio_mixer_view(shot->mixer).pending_count;
    state.complete.eagle_launched = true;
    state.complete.completing = false;
    CHECK(ab_classic_level_finish_failed(finish, 1, error) &&
          state.failed_timer == 2.625f && !state.complete.completing &&
          ab_audio_mixer_view(shot->mixer).pending_count == audio);
    state.complete.eagle_launched = false;
    CHECK(ab_classic_level_finish_failed(finish, 0, error) &&
          state.failed_timer == 2.625f && state.complete.completing &&
          ab_audio_mixer_view(shot->mixer).pending_count > audio &&
          damage_name_equal(field(observer.event, "id"), event_id) &&
          ab_event_manager_remove(shot->events, event_id, &listener, error));
    CHECK(lifecycle_ending(shot, shots, sling, scores, floating, finish, &state,
                           conditions, error));
    CHECK(lifecycle_start(shot, sling, error));
    CHECK(lifecycle_powerups(shot, shots, sling, error));
    ab_classic_level_finish_close(finish);
    ab_classic_sling_close(sling);
    return true;
}

typedef struct {
    AbClassicGoals *owner;
    AbEventManager *events;
    AbDataValue *notification_sent;
} LifecycleGoals;

typedef struct {
    AbDataValue *moving;
    size_t calls;
} LifecycleGoalMutation;

static bool lifecycle_goal_mutation(void *context, AbDataTable *event, AbError *error) {
    LifecycleGoalMutation *mutation = context;
    (void)error;
    CHECK(damage_name_equal(field(event, "id"),
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = span("EID_LEVEL_GOALS_CLEARED")}));
    *mutation->moving = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    ++mutation->calls;
    return true;
}

static bool bird_test_lifecycle(BirdCollisionTest *test, const AbZip *archive,
                                AbClassicBehaviors *behaviors,
                                AbClassicObjectRemoval *physical,
                                AbClassicRenderScene *scene, AbStringMap *hats,
                                AbClassicScores *root_scores,
                                AbFloatingScores *floating, AbClassicRemovals *removals,
                                LifecycleGoals goals, const AbSpriteProvider *provider,
                                AbError *error) {
    ShotTest *shot = test->shot;
    AbTargetData files[3] = {0};
    AbCameraSequence sequences[3] = {0};
    const char *const paths[] = {
        "assets/data/scripts/cameraSequences/FollowShotBirdEndCameraSequence.lua",
        "assets/data/scripts/cameraSequences/FollowShotBirdCameraSequence.lua",
        "assets/data/scripts/cameraSequences/LevelEndSequence.lua"};
    uint8_t key[32] = {0};
    bool okay = ab_target_resource_key(archive, 16 * 1024 * 1024, key, error);
    for (size_t index = 0; okay && index < 3; ++index) {
        okay = ab_target_data_open(archive, paths[index], (AbSpan){key, 32},
                                   1024 * 1024, NULL, files + index, error);
        AbDataValue sequence =
            okay ? field(files[index].data.globals, "sequence") : (AbDataValue){0};
        okay = okay && sequence.kind == AB_DATA_TABLE &&
               ab_camera_sequence_read(sequence.table, 65536, sequences + index, error);
    }
    memset(key, 0, sizeof(key));
    CHECK(okay);
    AbCameraSnap snaps[2];
    float gravity = shot->level.world->definition.gravity.y;
    AbClassicCamera *camera_owner = NULL;
    AbClassicShots *shots = NULL;
    AbClassicBirds *birds = NULL;
    AbClassicSling *sling = NULL;
    AbClassicCharacters *characters = NULL;
    AbClassicController *controller = NULL;
    AbClassicTrajectories *trajectories = NULL;
    LifecycleCameraResources camera_resources = {.sequences = sequences};
    CHECK(ab_camera_museum_snaps(shot->level.data->globals, (AbVec2){480, 320}, snaps,
                                 error) &&
          ab_classic_camera_open(
              &(AbClassicCameraConfig){.level = shot->owner,
                                       .snaps = snaps,
                                       .snap_count = 2,
                                       .pixel_size = {480, 320},
                                       .absolute_bottom = 200,
                                       .horizontal_leeway = 200,
                                       .gravity = &gravity,
                                       .shot_sequence = sequences + 1,
                                       .end_sequence = sequences,
                                       .resource_context = &camera_resources,
                                       .load_sequence = lifecycle_sequence,
                                       .sequence_log = lifecycle_camera_log,
                                       .sequence_capacity = sequences[2].count,
                                       .storage_bytes = 128 * 1024},
              &camera_owner, error) &&
          ab_classic_shots_open((AbClassicShotConfig){.level = shot->owner,
                                                      .visuals = test->visuals,
                                                      .behaviors = behaviors,
                                                      .camera = camera_owner,
                                                      .flying_object = &shot->flying,
                                                      .globals = &shot->globals,
                                                      .storage_bytes = 65536},
                                &shots, error));
    AbDataTable base_effect = {0};
    AbDataValue effect = {.kind = AB_DATA_TABLE, .table = &base_effect};
    /* Museum loadLevel prototype 2711 assigns 1 and 0x3b23d70a. */
    float remove_time = 1, threshold = 0.0025f;
    CHECK(bits(threshold) == UINT32_C(0x3b23d70a));
    CHECK(ab_classic_birds_open(
        (AbClassicBirdConfig){.level = shot->owner,
                              .shots = shots,
                              .behaviors = behaviors,
                              .camera = camera_owner,
                              .physical = physical,
                              .particles = test->particles,
                              .audio = shot->audio,
                              .hats = hats,
                              .flying_object = &shot->flying,
                              .specialty_available = &shot->globals.specialty_available,
                              .level_effect = &effect,
                              .remove_speed_squared = &threshold,
                              .remove_time = &remove_time,
                              .context = test,
                              .removed_hook = lifecycle_hook,
                              .removal_volume = lifecycle_volume,
                              .storage_bytes = 65536},
        &birds, error));
    AbClassicBirdView initial = ab_classic_birds_view(birds);
    AbObjectPropertyAccess old_other = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = initial.other_birds}};
    CHECK(ab_object_property_set(&old_other, span("retained"),
                                 (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                 error) &&
          ab_classic_birds_reset_other(birds, error));
    AbClassicBirdView replaced = ab_classic_birds_view(birds);
    CHECK(replaced.other_birds != initial.other_birds && !replaced.other_birds->count &&
          field(initial.other_birds, "retained").bits);
    AbShotState *state = ab_classic_shots_state(shots);
    AbShotBird *first = ab_classic_shots_bird(shots, test->first);
    CHECK(first);
    state->current_bird_name = first->name;
    state->flying = first;
    AbDataTable *object = &shot->level.properties->objects[test->first].physical;
    AbDataValue saved_shot = field(object, "shot");
    CHECK(ab_level_property_set(shot->level.properties, test->first, span("shot"),
                                (AbDataValue){0}, error));
    bool alive = false;
    size_t engine_particles =
        ab_particles_count(ab_classic_particles_engine(test->particles), false);
    CHECK(ab_classic_birds_update(birds, test->first, NAN, &alive, error) && alive &&
          field(object, "isReadyForRemoveTimer").kind == AB_DATA_NIL &&
          ab_particles_count(ab_classic_particles_engine(test->particles), false) ==
              engine_particles &&
          ab_level_property_set(shot->level.properties, test->first, span("shot"),
                                saved_shot, error) &&
          ab_level_property_set(
              shot->level.properties, test->first, span("isReadyForRemoveTimer"),
              (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(-0.01f)}, error) &&
          ab_level_property_set(shot->level.properties, test->first, span("xVel"),
                                (AbDataValue){.kind = AB_DATA_NUMBER}, error) &&
          ab_level_property_set(shot->level.properties, test->first, span("yVel"),
                                (AbDataValue){.kind = AB_DATA_NUMBER}, error));
    CHECK(ab_classic_trajectories_open(
        (AbClassicTrajectoryConfig){.level = shot->owner,
                                    .scene = scene,
                                    .tracks = 32,
                                    .points = 32,
                                    .storage_bytes = 65536},
        &trajectories, error));
    shot->active_trajectories = trajectories;
    shot->behavior_camera = camera_owner;
    CHECK(ab_classic_behaviors_launch(behaviors, test->first, (AbVec2){0}, error) &&
          ab_classic_trajectories_view(trajectories).tracks == 1 &&
          camera_resources.count == 2 &&
          ab_camera_system_cancel_sequence(ab_classic_camera_system(camera_owner),
                                           error) &&
          ab_camera_system_stop_movement(ab_classic_camera_system(camera_owner)));
    AbVec2 moved = {from_bits(field(object, "x").bits) + 2,
                    from_bits(field(object, "y").bits)};
    AbCameraShake shake;
    AbHeroScreenTinter hero;
    AbBurnManager burn = {0};
    AbClassicGameEffects effects = {0};
    AbClassicGamePrelude prelude = {0};
    AbDataValue cheats = {.kind = AB_DATA_BOOLEAN}, touch_count = {0},
                do_not_timeout = {0};
    AbVec2 cursor = {12, 34}, cursor_physics = {0}, cursor_world = {7, 8},
           old_cursor_world = {0};
    uint64_t before_effects_random = shot->audio_random.value;
    CHECK(
        shot->globals.time.bits == bits(3) &&
        shot->globals.last_score_time.bits == bits(3) &&
        ab_classic_game_globals_advance_time(&shot->globals, 1, error) &&
        shot->globals.time.bits == bits(4) &&
        shot->globals.last_score_time.bits == bits(3) && ab_camera_shake_init(&shake) &&
        ab_hero_screen_tinter_init(&hero) && ab_burn_manager_add(&burn, 2, error) &&
        ab_classic_game_effects_initialize(
            (AbClassicGameEffectConfig){&shake, &shot->audio_random, &hero,
                                        trajectories, &burn},
            &effects, error) &&
        ab_classic_camera_update(camera_owner, 0, &(AbCameraSystemInput){0}, error) &&
        ab_classic_game_prelude_initialize(
            (AbClassicGamePreludeConfig){.shots = shots,
                                         .camera = camera_owner,
                                         .effects = &effects,
                                         .settings = shot->statistics,
                                         .flying = &shot->flying,
                                         .cheats_enabled = &cheats,
                                         .touch_count = &touch_count,
                                         .do_not_timeout = &do_not_timeout,
                                         .cursor = &cursor,
                                         .cursor_physics = &cursor_physics,
                                         .cursor_world = &cursor_world,
                                         .old_cursor_world = &old_cursor_world},
            &prelude, error) &&
        ab_classic_level_set_position(shot->owner, test->first, moved, error) &&
        ab_classic_game_prelude_update(&prelude, 0.125f, shot->globals.time, error) &&
        !shot->globals.level_timeout.bits &&
        shot->globals.last_score_timer.bits == bits(4) &&
        shot->globals.time_spent.bits == bits(0.125f) &&
        !shot->globals.extra_tutorial_timer.bits && old_cursor_world.x == 7 &&
        old_cursor_world.y == 8 && isfinite(cursor_physics.x) &&
        isfinite(cursor_world.x) && shot->audio_random.value == before_effects_random &&
        !hero.tween && hero.finished && burn.deadline_present && burn.deadline == 6 &&
        ab_classic_trajectories_view(trajectories).points == 1 &&
        ab_classic_render_scene_view(scene).extra_count == 1);
    AbDataValue tutorials =
        field(ab_settings_statistics_root(shot->statistics), "tutorials");
    CHECK(tutorials.kind == AB_DATA_TABLE && tutorials.table &&
          !tutorials.table->count);
    AbObjectDefinition tutorial_definition;
    CHECK(ab_object_definition_open(
        shot->level.catalog, shot->level.bodies->objects[test->first].definition_name,
        NULL, &tutorial_definition, error));
    AbDataValue tutorial_sprite = field(tutorial_definition.source, "sprite");
    CHECK(tutorial_sprite.kind == AB_DATA_STRING);
    AbSpan tutorial_path[] = {span("tutorials"), tutorial_sprite.string,
                              span("showHelp")};
    CHECK(ab_settings_statistics_create_table(shot->statistics, tutorial_path, 2, 1,
                                              error) &&
          ab_settings_statistics_write(
              shot->statistics, tutorial_path, 3,
              (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) &&
          ab_classic_game_prelude_update(&prelude, 0.125f, shot->globals.time, error) &&
          shot->globals.extra_tutorial_timer.bits == bits(0.125f) &&
          shot->globals.time_spent.bits == bits(0.25f) && burn.deadline == 6 &&
          ab_classic_trajectories_view(trajectories).points == 1);
    const AbLuaAssetRenderer *trail =
        ab_classic_render_scene_view(scene).extra_renderers;
    CHECK(trail[0].sprite_name.size == 13 &&
          !memcmp(trail[0].sprite_name.data, "TRAIL_WHITE_1", 13) &&
          trail[0].sprite.region && trail[0].pose.world_position.x == moved.x * 20);
    CHECK(ab_classic_trajectories_puff(trajectories, (AbVec2){24, -30},
                                       (AbDataValue){0}, (AbDataValue){0},
                                       (AbDataValue){0}, error) &&
          ab_classic_render_scene_view(scene).extra_count == 2 &&
          trail[0].id.value != trail[1].id.value &&
          trail[0].id.script_number == trail[1].id.script_number &&
          ab_classic_trajectories_view(trajectories).points == 1 &&
          ab_classic_trajectories_view(trajectories).retained_points == 2 &&
          trail[1].layer == 3 && trail[1].sprite.region);
    AbGameObjectCoreRegistry *registry = ab_classic_object_removal_registry(physical);
    size_t native_birds = registry->bird_count;
    size_t members = ab_classic_render_scene_view(scene).count;
    size_t audio = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(ab_classic_sling_open(
        (AbClassicSlingConfig){.level = shot->owner,
                               .shots = shots,
                               .behaviors = behaviors,
                               .services = shot->bindings,
                               .events = shot->events,
                               .audio = shot->audio,
                               .camera = ab_classic_camera_system(camera_owner),
                               .start = {.level_start = shot->rest, .band = shot->rest},
                               .physics_scale = shot->physics_scale,
                               .default_force = shot->default_force,
                               .current_bird_index = (float)shot->level.bird_count + 1,
                               .storage_bytes = 65536},
        &sling, error));
    AbClassicCharacterServices services = {0};
    AbDataValue prevent = {0}, moving = {0}, no_wait = {0}, eagle = {0},
                eagle_timer = {0};
    AbDataValue bird_sound = {.kind = AB_DATA_STRING, .string = span("bird_misc")};
    AbDataValue pig_sound = {.kind = AB_DATA_STRING, .string = span("piglette")};
    CHECK(ab_classic_character_services_initialize(
        (AbClassicCharacterServiceConfig){.level = shot->owner,
                                          .camera = camera_owner,
                                          .sling = sling,
                                          .birds = birds,
                                          .audio = shot->audio,
                                          .prevent_ending = &prevent,
                                          .level_timeout = &shot->globals.level_timeout,
                                          .moving_objects = &moving,
                                          .do_not_wait_for_moving_objects = &no_wait,
                                          .bird_sound = &bird_sound,
                                          .pig_sound = &pig_sound,
                                          .goals = goals.owner,
                                          .do_not_timeout = &do_not_timeout,
                                          .eagle_launched = &eagle,
                                          .eagle_timer = &eagle_timer},
        &services, error));
    AbClassicCharacterHost character_host =
        ab_classic_character_services_host(&services);
    bool lost = true;
    CHECK(character_host.lose_condition(character_host.context, &lost, error) && !lost);
    shot->globals.level_timeout = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(character_host.lose_condition(character_host.context, &lost, error) && lost);
    prevent = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(character_host.lose_condition(character_host.context, &lost, error) && !lost);
    prevent = (AbDataValue){0};
    shot->globals.level_timeout = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    bool won = false;
    CHECK(ab_classic_character_services_win(&services, &won, error) && won);
    moving = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    no_wait = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(1)};
    CHECK(ab_classic_character_services_win(&services, &won, error) && !won);
    do_not_timeout = (AbDataValue){.kind = AB_DATA_NUMBER};
    CHECK(ab_classic_character_services_win(&services, &won, error) && won);
    do_not_timeout = (AbDataValue){0};
    no_wait = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_character_services_win(&services, &won, error) && won);
    eagle = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_character_services_win(&services, &won, error) && !won);
    eagle_timer = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(0.125f)};
    CHECK(ab_classic_character_services_win(&services, &won, error) && !won);
    eagle_timer = (AbDataValue){.kind = AB_DATA_NUMBER};
    CHECK(ab_classic_character_services_win(&services, &won, error) && won);
    eagle = (AbDataValue){.kind = AB_DATA_NUMBER};
    eagle_timer = (AbDataValue){0};
    CHECK(ab_classic_character_services_win(&services, &won, error) && won);
    prevent = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_character_services_win(&services, &won, error) && !won);
    prevent = (AbDataValue){0};
    moving = (AbDataValue){0};
    no_wait = (AbDataValue){0};
    /* Explicitly reset the sent flag for this callback-observation control.
     * The earlier natural destruction path has already notified exactly once. */
    *goals.notification_sent = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    LifecycleGoalMutation mutation = {&moving, 0};
    AbEventListener goal_listener = {span("WinQueryMutation"), &mutation,
                                     lifecycle_goal_mutation};
    AbDataValue goal_id = {.kind = AB_DATA_STRING,
                           .string = span("EID_LEVEL_GOALS_CLEARED")};
    CHECK(ab_event_manager_add(goals.events, goal_id, &goal_listener, error) &&
          ab_classic_character_services_win(&services, &won, error) && !won &&
          mutation.calls == 1 && moving.bits && goals.notification_sent->bits &&
          ab_event_manager_remove(goals.events, goal_id, &goal_listener, error));
    moving = (AbDataValue){0};
    CHECK(ab_classic_character_services_win(&services, &won, error) && won &&
          mutation.calls == 1);
    CHECK(ab_classic_characters_open(shot->owner, test->visuals, &shot->audio_random,
                                     character_host, 65536, &characters, error));
    size_t unshot = 0;
    while (unshot < shot->level.bodies->count &&
           (unshot == test->first || shot->level.removed_objects[unshot] ||
            !shot->level.bodies->objects[unshot].physics.controllable))
        ++unshot;
    CHECK(unshot < shot->level.bodies->count);
    AbCharacterAnimationObject *animated = shot->level.character_objects + unshot;
    bool saved_shot_flag = animated->shot;
    AbDataValue saved_shot_value =
        field(&shot->level.properties->objects[unshot].physical, "shot");
    animated->shot = false;
    animated->bird.timer = -0.01f;
    animated->jump.timer = 100;
    CHECK(ab_level_property_set(shot->level.properties, unshot, span("shot"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN}, error));
    float live_distance = -1;
    CHECK(character_host.camera_distance(character_host.context, unshot,
                                         AB_CLASSIC_CHARACTER_BIRD, &live_distance,
                                         error) &&
          isfinite(live_distance) && live_distance >= 0);
    size_t before_character_audio = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(character_host.play_sound(character_host.context, unshot,
                                    AB_CLASSIC_CHARACTER_BIRD, 0.75f, error) &&
          ab_audio_mixer_view(shot->mixer).pending_count > before_character_audio);
    AbGameTailState tail = {.cursor = {12, 34}, .time_since_present = true};
    AbBlockScriptManager block_scripts = {0};
    AbBlockScriptUpdate block_update = {.event = span("onUpdateBlock")};
    AbClassicControllerEffects level_effects = {0};
    bool physics_enabled = true;
    AbClassicLevelFinishState full_finish_state = {0};
    bool *level_completed = &full_finish_state.complete.completed;
    LifecycleInputBinding input_binding = {0};
    CHECK(lifecycle_input_binding(shot, shots, sling, behaviors,
                                  ab_classic_camera_system(camera_owner), &tail,
                                  level_completed, &input_binding, error) &&
          ab_classic_game_input_update(&input_binding.input,
                                       (AbClassicGameTouchInput){0}, 0.125f,
                                       shot->globals.time, error));
    float root_total = 0;
    AbDrawCommand commands[128];
    AbDrawList draw_list = {commands, 0, 128};
    LifecycleGameDraw gameplay_draw = {0};
    CHECK(lifecycle_game_draw_open(shot, archive, shots, sling, test->visuals,
                                   behaviors, scene, camera_owner, provider,
                                   ab_classic_particles_engine(test->particles),
                                   &gameplay_draw, error) &&
          lifecycle_sling_draw(shot, shots, gameplay_draw.sling, test->first, error));
    LifecycleRender render = {
        .list = &draw_list, .input = &input_binding, .game_draw = &gameplay_draw.owner};
    CHECK(ab_classic_controller_open(
        (AbClassicControllerConfig){.level = shot->owner,
                                    .shots = shots,
                                    .sling = sling,
                                    .birds = birds,
                                    .behaviors = behaviors,
                                    .camera = camera_owner,
                                    .characters = characters,
                                    .scene = scene,
                                    .removals = removals,
                                    .floating = floating,
                                    .scores = root_scores,
                                    .blocks = &block_scripts,
                                    .block_update = &block_update,
                                    .level_effects = &level_effects,
                                    .effect_kind = AB_CLASSIC_CONTROLLER_EFFECT_BASE,
                                    .state = &tail,
                                    .score = &root_total,
                                    .physics_enabled = &physics_enabled,
                                    .level_completed = level_completed,
                                    .wobble_steps = 8,
                                    .context = &render,
                                    .service = lifecycle_render,
                                    .input = &input_binding.input,
                                    .storage_bytes = 65536},
        &controller, error));
    AbCameraSystemInput camera_input = {.shake = shake.offset};
    AbGameTailObject *stable_controller_object =
        ab_classic_controller_object(controller, test->first);
    CHECK(stable_controller_object &&
          !ab_classic_controller_register_object(controller, test->first, NULL) &&
          !ab_classic_controller_register_object(controller,
                                                 shot->level.bodies->count + 1, NULL) &&
          ab_classic_controller_object(controller, test->first) ==
              stable_controller_object);
    CHECK(ab_classic_controller_update(controller, 0.125f, 4, &camera_input, error) &&
          root_total == 5640 && state->score == root_total && state->time == 4 &&
          shot->globals.score.bits == bits(5640) && state->last_score_time == 3 &&
          tail.old_cursor.x == 12 && tail.old_cursor.y == 34 &&
          tail.time_since_start == 0.125f && render.draws == 1 && draw_list.count &&
          input_binding.state.skip_input.kind == AB_DATA_BOOLEAN &&
          !input_binding.state.skip_input.bits &&
          input_binding.state.skip_trigger_specialty.kind == AB_DATA_BOOLEAN &&
          !input_binding.state.skip_trigger_specialty.bits &&
          ab_classic_controller_object(controller, test->first)->identity ==
              test->first);
    AbError cutoff = {0};
    CHECK(lifecycle_game_draw_checks(&gameplay_draw, &draw_list, error));
    unsigned update_phase = 0;
    render.phase = &update_phase;
    input_binding.state.skip_trigger_specialty = (AbDataValue){.kind = AB_DATA_NUMBER};
    float menu_particle_scale = 1;
    AbClassicFrameSpotlight *spotlight = NULL;
    AbParticleDrawStyle particle_style = {.provider = provider,
                                          .width = 480,
                                          .height = 320,
                                          .color = {1, 1, 1, 1},
                                          .opacity = 1};
    AbClassicFrameParticles particle_frame = {0};
    CHECK(ab_classic_frame_particles_initialize(
        (AbClassicFrameParticleConfig){ab_classic_particles_engine(test->particles),
                                       &physics_enabled, &menu_particle_scale,
                                       &particle_style, &draw_list, &spotlight},
        &particle_frame, error));
    CHECK(ab_particles_count(ab_classic_particles_engine(test->particles), false) ==
          engine_particles + 8);
    CHECK(lifecycle_game_update(shot, shots, camera_owner, controller, &input_binding,
                                &prelude, &services, root_scores, floating,
                                ab_classic_particles_engine(test->particles),
                                &physics_enabled, &full_finish_state, &update_phase,
                                test->museum.config.dead_blocks, &particle_frame,
                                &gameplay_draw.owner, error) &&
          render.draws == 2 && tail.time_since_start == 0.125f && root_total == 5640);
    /* The native zero-time post-root pass can expire positive-y particles. */
    size_t retained_particles =
        ab_particles_count(ab_classic_particles_engine(test->particles), false);
    render.phase = NULL;
    render.fail_draw = true;
    input_binding.state.cursor = (AbVec2){40, 50};
    input_binding.state.skip_trigger_specialty = (AbDataValue){.kind = AB_DATA_NUMBER};
    CHECK(ab_classic_game_input_update(&input_binding.input,
                                       (AbClassicGameTouchInput){0}, 0,
                                       shot->globals.time, error));
    CHECK(!ab_classic_controller_update(controller, 0, 4, &camera_input, &cutoff) &&
          strstr(cutoff.message, "Intentional controller draw failure") &&
          render.draws == 3 && tail.time_since_start == 0.125f && root_total == 5640 &&
          tail.old_cursor.x == 40 && tail.old_cursor.y == 50 &&
          input_binding.state.skip_input.bits &&
          input_binding.state.skip_trigger_specialty.kind == AB_DATA_NUMBER &&
          !ab_classic_controller_update(controller, 0.125f, 4, &camera_input, NULL) &&
          render.draws == 3);
    cheats.bits = 1;
    touch_count = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(6)};
    AbError missing_service = {0};
    uint64_t before_failure_random = shot->audio_random.value;
    CHECK(!ab_classic_game_prelude_update(&prelude, 0.125f, shot->globals.time,
                                          &missing_service) &&
          strstr(missing_service.message, "Missing actual toy-activation") &&
          prelude.failed && !prelude.updating &&
          shot->globals.time_spent.bits == bits(0.375f) &&
          shot->audio_random.value == before_failure_random &&
          !ab_classic_game_prelude_update(&prelude, 0.125f, shot->globals.time, NULL) &&
          shot->globals.time_spent.bits == bits(0.375f));
    animated->shot = saved_shot_flag;
    CHECK(ab_level_property_set(shot->level.properties, unshot, span("shot"),
                                saved_shot_value, error));
    alive = !shot->level.removed_objects[test->first];
    CHECK(!alive && registry->bird_count == native_birds - 1 &&
          !ab_classic_trajectories_view(trajectories).tracks &&
          shot->level.removed_objects[test->first] &&
          !ab_world_has_body(shot->level.world,
                             shot->level.bodies->objects[test->first].physics.body) &&
          !state->current_bird_name && !state->flying &&
          shot->flying.kind == AB_DATA_NIL && !shot->globals.specialty_available.bits &&
          ab_classic_camera_view(camera_owner).flying_object == SIZE_MAX &&
          !ab_classic_trajectories_view(trajectories).points &&
          ab_classic_render_scene_view(scene).count == members - 2 &&
          ab_particles_count(ab_classic_particles_engine(test->particles), false) ==
              retained_particles &&
          ab_audio_mixer_view(shot->mixer).pending_count > audio);
    AbClassicBirdView view = ab_classic_birds_view(birds);
    AbDataValue name = {.kind = AB_DATA_STRING,
                        .string = shot->level.bodies->objects[test->first].name};
    const AbDataValue *member = ab_data_get(view.birds, name);
    CHECK((!member || member->kind == AB_DATA_NIL) &&
          field(object, "isReadyForRemoveTimer").bits == bits(-0.01f - 0.125f) &&
          ab_classic_birds_update(birds, test->first, 0.125f, &alive, error) &&
          !alive && registry->bird_count == native_birds - 1);
    CHECK(field(initial.other_birds, "retained").bits &&
          ab_classic_birds_view(birds).other_birds == replaced.other_birds);
    size_t members_before_keep = ab_classic_render_scene_view(scene).count;
    AbDataValue keep = {.kind = AB_DATA_TABLE, .table = object};
    CHECK(ab_classic_trajectories_keep(trajectories, &keep, 1, error) &&
          !ab_classic_trajectories_view(trajectories).points &&
          ab_classic_render_scene_view(scene).count == members_before_keep &&
          ab_classic_trajectories_clear(trajectories, error) &&
          ab_classic_render_scene_view(scene).count == members_before_keep);
    /* Rounded ID replacement leaves the earlier actual scene renderer alive,
     * exactly as the original numeric registry/native disposal split does. */
    bool retained_trail = false;
    AbClassicRenderSceneView retained = ab_classic_render_scene_view(scene);
    for (size_t index = 0; index < retained.count; ++index)
        retained_trail |= retained.members[index].object == trail;
    CHECK(retained_trail);
    CHECK(lifecycle_finish(shot, shots, behaviors, camera_owner, sequences + 2,
                           root_scores, floating, &services, error) &&
          camera_resources.count == 10 &&
          lifecycle_specialty(shot, shots, behaviors, sling,
                              ab_classic_camera_system(camera_owner), error) &&
          lifecycle_level_particles(test->particles, provider, error));
    shot->active_trajectories = NULL;
    shot->behavior_camera = NULL;
    ab_classic_trajectories_close(trajectories);
    ab_classic_controller_close(controller);
    lifecycle_game_draw_close(&gameplay_draw);
    lifecycle_input_binding_close(&input_binding);
    ab_classic_characters_close(characters);
    ab_classic_sling_close(sling);
    ab_classic_birds_close(birds);
    ab_classic_shots_close(shots);
    ab_classic_camera_close(camera_owner);
    for (size_t index = 0; index < 3; ++index) {
        ab_camera_sequence_free(sequences + index);
        ab_target_data_close(files + index);
    }
    return true;
}

#endif
