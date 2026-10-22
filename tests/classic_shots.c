#include "ab/classic_shots.h"
#include "ab/slingshot.h"
#include "ab/shot_services.h"
#include "ab/classic_shot_listeners.h"
#include "ab/audio_bank.h"
#include "ab/target_resources.h"
#include "ab/classic_shot_effects.h"
#include "ab/classic_sling.h"
#include "ab/classic_launch_host.h"
#include "ab/classic_audio.h"
#include "ab/classic_trajectories.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Classic shot failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbClassicLevel *owner;
    AbClassicLevelView level;
    AbClassicShots *shots;
    AbClassicSling *sling;
    AbClassicTrajectories *active_trajectories;
    AbClassicCamera *behavior_camera;
    AbCameraSystem *camera;
    AbDataValue flying;
    AbClassicGameGlobals globals;
    size_t bird, services, behaviors;
    AbVec2 direction, band;
    float stretch, power;
    AbSettingsStatistics *statistics;
    AbEventManager *events;
    AbShotServices *bindings;
    AbClassicLaunchHost launch_host;
    AbDataValue launch_powerups_enabled, launch_red_special;
    AbDataValue launch_next_sound;
    AbClassicShotListeners *game_listeners;
    AbAudioBank *audio;
    AbAudioMixer *mixer;
    AbPowerupUses *powerups;
    AbRandom48 audio_random;
    bool launch_cue;
    AbEventListener observer;
    AbDataEntry vector_entries[4];
    AbDataTable vectors[2];
    const AbDataTable *notification;
    size_t notification_calls;
    AbVec2 rest;
    float default_force, physics_scale;
    size_t reload_sounds;
} ShotTest;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float from_bits(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *key) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(key)});
    return value ? *value : (AbDataValue){0};
}

static bool shot_dynamic_registry(ShotTest *test) {
    AbLevelProperties *properties = test->level.properties;
    CHECK(properties &&
          field(&properties->registry, "world").table == &properties->world &&
          field(&properties->registry, "dynamic").table == &properties->dynamic &&
          properties->dynamic.count >= test->level.bird_count + test->level.goal_count);
    for (size_t order = 0; order < test->level.bird_count; ++order) {
        size_t index = test->level.bird_order[order];
        const AbDataValue *value = ab_data_get(
            &properties->dynamic,
            (AbDataValue){.kind = AB_DATA_STRING,
                          .string = test->level.bodies->objects[index].name});
        size_t body = test->level.bodies->objects[index].physics.body;
        CHECK(value && value->kind == AB_DATA_TABLE &&
              value->table == &properties->objects[index].physical &&
              test->level.world->storage.bodies[body].motion.mass.type ==
                  AB_BODY_STATIC);
    }
    size_t pig = test->level.goal_order[0];
    const AbDataValue *goal =
        ab_data_get(&properties->dynamic,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = test->level.bodies->objects[pig].name});
    CHECK(goal && goal->table == &properties->objects[pig].physical &&
          field(&properties->dynamic, "ground").kind == AB_DATA_NIL);
    return true;
}

#include "classic_damage_visuals.h"
#include "classic_block_collisions.h"
#include "classic_bird_collisions.h"

/* Real ordinary shot services. The fixture explicitly selects the ordinary
 * red-special-disabled, unpowered, default-slingshot state. Other modes and
 * powered/special effects remain unsupported in their runtime adapters. */
static bool service(void *context, AbShotCall *call, AbError *error) {
    (void)error;
    ShotTest *test = context;
    ++test->services;
    if (test->launch_host.initialized)
        return ab_classic_launch_host_remaining(&test->launch_host, call, error);
    switch (call->operation) {
        case AB_SHOT_DEFINITION_HOOK:
            CHECK(!strcmp(call->text, "redBirdShot") &&
                  test->flying.table ==
                      &test->level.properties->objects[test->bird].physical &&
                  field(&test->level.properties->objects[test->bird].physical,
                        "startImpulseX")
                          .kind == AB_DATA_NUMBER);
            return ab_classic_red_bird_shot(test->owner, test->bird, false, error);
        case AB_SHOT_ATTACH_PARTICLES:
            CHECK(test->level.character_objects[test->bird].shot &&
                  test->globals.extra_tutorial_timer.kind == AB_DATA_NUMBER &&
                  !test->globals.extra_tutorial_timer.bits &&
                  test->globals.specialty_available.bits &&
                  field(&test->level.properties->objects[test->bird].physical, "shot")
                          .bits == 1);
            return ab_classic_shot_attach_particles(test->owner, test->bird,
                                                    test->powerups, true, error);
        case AB_SHOT_RESOLVE_AUDIO:
            CHECK(!strcmp(call->text, "bird_01_flying"));
            return ab_audio_bank_name(test->audio, call->text, &call->text, error);
        case AB_SHOT_PLAY_AUDIO: {
            CHECK(call->value == 1 && !strcmp(call->text, "bird_01_flying"));
            uint32_t handle;
            CHECK(ab_audio_bank_play(
                test->audio,
                (AbAudioPlay){
                    .name = {.kind = AB_DATA_STRING, .string = span(call->text)},
                    .volume = {.kind = AB_DATA_NUMBER, .bits = bits(call->value)}},
                &handle, error));
            CHECK(handle != UINT32_MAX);
            return true;
        }
        case AB_SHOT_PLAY_SLING_SOUND: {
            uint32_t handle;
            CHECK(ab_audio_bank_default_sling_launch(test->audio, &handle, error));
            test->launch_cue = true;
            ab_classic_sling_set_launch_cue(test->sling);
            return true;
        }
        default:
            CHECK(false);
    }
}

static bool sling_query(void *context, AbSlingLaunchCall *call, AbError *error) {
    (void)error;
    ShotTest *test = context;
    switch (call->operation) {
        case AB_SLING_LAUNCH_DIRECTION:
            call->direction = &test->direction;
            return true;
        case AB_SLING_LAUNCH_DISTANCE: {
            float squared = test->direction.x * test->direction.x +
                            test->direction.y * test->direction.y;
            call->number = (float)sqrt((double)squared);
            return true;
        }
        case AB_SLING_LAUNCH_STRETCH:
            call->number = test->stretch;
            return true;
        case AB_SLING_LAUNCH_POWER:
            call->number = test->power;
            return true;
        case AB_SLING_LAUNCH_POSITION:
            call->position = &test->band;
            return true;
        default:
            CHECK(false);
    }
}

static bool behavior(void *context, const AbMuseumBirdCall *call, AbError *error) {
    (void)error;
    ShotTest *test = context;
    if (test->active_trajectories) {
        ++test->behaviors;
        if (call->operation == AB_MUSEUM_CAMERA_SEQUENCE)
            return ab_classic_camera_start_sequence(test->behavior_camera,
                                                    call->sequence, error);
        return ab_classic_trajectories_museum(test->active_trajectories, call, error);
    }
    CHECK(field(&test->level.properties->objects[test->bird].physical, "launchVelocity")
              .kind == AB_DATA_NIL);
    if (call->operation == AB_MUSEUM_TRAJECTORY_ADD) {
        CHECK(call->object.table == test->flying.table);
    } else {
        CHECK(call->operation == AB_MUSEUM_CAMERA_SEQUENCE &&
              !strcmp(call->sequence, "FollowShotBirdCameraSequence"));
    }
    ++test->behaviors;
    return true;
}

static bool notification(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    ShotTest *test = context;
    if (test->sling) {
        CHECK(test->flying.table &&
              field(event, "birdsShooted").bits ==
                  bits(12 + (float)test->notification_calls) &&
              field(event, "backwardsBirdCount").bits == bits(3) &&
              field(event, "launchPosition").kind == AB_DATA_TABLE &&
              field(event, "launchImpulse").kind == AB_DATA_TABLE &&
              test->notification != event);
        test->notification = event;
        ++test->notification_calls;
        return true;
    }
    CHECK(!test->flying.table && field(event, "birdsShooted").bits == bits(12) &&
          field(event, "backwardsBirdCount").bits == bits(3) &&
          field(event, "launchPosition").table == test->vectors &&
          field(event, "launchImpulse").table == test->vectors + 1);
    if (test->notification_calls)
        CHECK(test->notification == event);
    test->notification = event;
    ++test->notification_calls;
    return true;
}

static bool shot_dispatch(void *context, AbShotCall *call, AbError *error) {
    ShotTest *test = context;
    if (test->launch_host.initialized)
        return ab_classic_launch_host_dispatch(&test->launch_host, call, error);
    if (call->operation == AB_SHOT_NOTIFY_ALL) {
        AbShotState *state = ab_classic_shots_state(test->shots);
        CHECK(test->globals.last_score_time.bits == bits(state->time) &&
              test->globals.old_score.bits == bits(state->score));
    }
    return test->sling ? ab_classic_sling_shot(test->sling, call, error)
                       : ab_shot_services_shot(test->bindings, call, error);
}

static bool powerup_query(void *context, const char *name, AbDataValue *output,
                          AbError *error) {
    bool used = false;
    if (!ab_powerup_uses_contains(((ShotTest *)context)->powerups, name, &used, error))
        return false;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = used};
    return true;
}

static bool services_open(ShotTest *test, const AbZip *archive, AbVec2 impulse,
                          AbError *error) {
    static const AbCameraSnap snaps[] = {{{0, 0}, 1}, {{1000, 0}, 1}};
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){480, 320}, 1000, 0, 65536,
                                &test->camera, error));
    CHECK(ab_powerup_uses_open(32, 64, 4096, &test->powerups, error));
    CHECK(ab_random48_seed(&test->audio_random, 7) &&
          ab_audio_mixer_open((AbAudioMixerLimits){32, 512, 1, 256 * 1024},
                              &test->mixer, error));
    uint8_t key[32] = {0};
    bool audio_ready =
        ab_target_resource_key(archive, 16 * 1024 * 1024, key, error) &&
        ab_audio_bank_open(archive, (AbSpan){key, sizeof(key)},
                           (AbAudioBankConfig){test->mixer, &test->audio_random,
                                               64 * 1024 * 1024, 4 * 1024 * 1024,
                                               4 * 1024 * 1024},
                           &test->audio, error);
    memset(key, 0, sizeof(key));
    CHECK(audio_ready && ab_audio_mixer_start(test->mixer, true, error));
    CHECK(ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 8, 8, 65536},
        &test->statistics, error));
    const char *counter_names[] = {"birdsShooted", "backwardsBirdCount"};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(ab_settings_statistics_set_local(
            test->statistics, span(counter_names[index]),
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(index ? 1 : 8)}, error));
        AbSpan path[] = {span(counter_names[index]), span("device_fixture"),
                         span("installationId_previous")};
        CHECK(ab_settings_statistics_write(
            test->statistics, path, 3,
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(index ? 2 : 3)}, error));
    }
    CHECK(ab_event_manager_open((AbEventManagerLimits){16, 4, 8, 0, 8, 32768},
                                (AbEventManagerHost){0}, &test->events, error));
    CHECK(ab_classic_shot_listeners_open(
        (AbClassicShotListenerConfig){test->statistics,
                                      test->events,
                                      {.context = test, .powerup_used = powerup_query},
                                      8,
                                      16384},
        &test->game_listeners, error));
    AbClassicShotListenerState *listener_state =
        ab_classic_shot_listeners_state(test->game_listeners);
    listener_state->current_bird_index = 1;
    listener_state->ios = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    listener_state->powerups_enabled =
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_shot_listeners_bind(test->game_listeners, error));
    test->observer = (AbEventListener){span("fixture"), test, notification};
    CHECK(ab_event_manager_add(
        test->events,
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("EID_BIRD_SHOT")},
        &test->observer, error));
    CHECK(ab_shot_services_open((AbShotServicesConfig){test->statistics, test->events,
                                                       service, sling_query, test, 8,
                                                       8192},
                                &test->bindings, error));
    AbVec2 values[] = {test->band, impulse};
    for (size_t index = 0; index < 2; ++index) {
        test->vectors[index] =
            (AbDataTable){test->vector_entries + 2 * index, 2, 2, NULL};
        test->vectors[index].entries[0] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = span("x")},
                          {.kind = AB_DATA_NUMBER, .bits = bits(values[index].x)}};
        test->vectors[index].entries[1] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = span("y")},
                          {.kind = AB_DATA_NUMBER, .bits = bits(values[index].y)}};
    }
    CHECK(ab_shot_services_bind_vectors(test->bindings, test->vectors,
                                        test->vectors + 1, error));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%8" SCNx32, output + index) == 1);
    return true;
}

static bool launch_check(ShotTest *test, const AbZip *archive, AbClassicLevel *level,
                         AbClassicVisuals *visuals, AbClassicBehaviors *behaviors,
                         FILE *native, AbError *error) {
    char marker[80];
    uint32_t controls[8], data[17], expected[5];
    CHECK(fgets(marker, sizeof(marker), native) &&
          !strcmp(marker, "AB_LIVE_BIRD_SHOT_V1 1\n") &&
          read_words(native, controls, 8) && read_words(native, data, 17) &&
          read_words(native, expected, 5));
    AbSlingLaunchInput launch = {
        .rest = {from_bits(controls[0]), from_bits(controls[1])},
        .rubber_band = {from_bits(data[11]), from_bits(data[12])},
        .stretch = from_bits(controls[6]),
        .launch_force = from_bits(controls[3]),
        .physics_scale = from_bits(controls[4]),
        .bird_mass = from_bits(controls[7]),
        .slingshot_power = from_bits(controls[5]),
        .bird_present = true};
    AbVec2 impulse, position = launch.rubber_band;
    CHECK(ab_slingshot_launch_impulse(&launch, NULL, &impulse) &&
          bits(impulse.x) == data[13] && bits(impulse.y) == data[14]);
    test->globals =
        (AbClassicGameGlobals){.time = {.kind = AB_DATA_NUMBER},
                               .score = {.kind = AB_DATA_NUMBER},
                               .last_score_time = {.kind = AB_DATA_NUMBER},
                               .old_score = {.kind = AB_DATA_NUMBER},
                               .extra_tutorial_timer = {.kind = AB_DATA_NUMBER},
                               .specialty_available = {.kind = AB_DATA_BOOLEAN}};
    AbClassicShotConfig config = {.level = level,
                                  .visuals = visuals,
                                  .behaviors = behaviors,
                                  .flying_object = &test->flying,
                                  .context = test,
                                  .storage_bytes = 65536,
                                  .globals = &test->globals};
    CHECK(!ab_classic_shots_open((AbClassicShotConfig){0}, &test->shots, NULL) &&
          !test->shots && ab_classic_shots_open(config, &test->shots, error));
    size_t body_index = test->level.bodies->objects[test->bird].physics.body;
    CHECK(
        ab_world_set_body_type(test->level.world, body_index, AB_BODY_DYNAMIC, error));
    AbShotState *state = ab_classic_shots_state(test->shots);
    state->dragged = ab_classic_shots_bird(test->shots, test->bird);
    CHECK(state->dragged && bits(state->dragged->mass) == controls[7]);
    /* No external service: earlier real setters survive, but no launch impulse,
     * flags or behavior callback may be invented. */
    AbBodyMotion *body = &test->level.world->storage.bodies[body_index].motion;
    CHECK(ab_body_velocity_set(body, (AbVec2){5, 4}) &&
          !ab_classic_shots_launch(test->shots, &position, &impulse, NULL) &&
          body->mass.linear_velocity.x == 0 && body->mass.linear_velocity.y == 0 &&
          state->dragged && !state->dragged->shot && !test->behaviors &&
          field(&test->level.properties->objects[test->bird].physical, "startImpulseX")
                  .kind == AB_DATA_NIL);
    ab_classic_shots_close(test->shots);
    test->shots = NULL;
    test->band = position;
    CHECK(services_open(test, archive, impulse, error));
    config.service = shot_dispatch;
    config.context = test;
    CHECK(ab_classic_shots_open(config, &test->shots, error));
    state = ab_classic_shots_state(test->shots);
    AbShotBird *bird = ab_classic_shots_bird(test->shots, test->bird);
    *state = (AbShotState){.dragged = bird,
                           .bird_count = 3,
                           .bird_reload_delay = 0.5f,
                           .bird_ready = true};
    test->globals.extra_tutorial_timer =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(0.75f)};
    test->band = position;
    test->direction = (AbVec2){launch.rest.x - position.x, launch.rest.y - position.y};
    test->stretch = launch.stretch;
    test->power = launch.slingshot_power;
    test->rest = launch.rest;
    test->default_force = launch.launch_force;
    test->physics_scale = launch.physics_scale;
    AbSlingLaunchState sling = {.default_force = launch.launch_force,
                                .physics_scale = launch.physics_scale};
    CHECK(ab_classic_shots_launch_sling(test->shots, &sling, NULL,
                                        ab_shot_services_sling, test->bindings,
                                        error) &&
          !sling.dragged);
    CHECK(!state->dragged && state->flying == bird && bird->shot &&
          !bird->has_collided && state->birds_shot == 1 && !state->bird_ready &&
          state->bird_fired && state->next_bird_timer == 0.5f && test->services == 5 &&
          test->behaviors == 2 && test->notification_calls == 1 &&
          ab_shot_services_notification_count(test->bindings) == 1 &&
          test->launch_cue && ab_audio_mixer_view(test->mixer).pending_count == 2);
    float total;
    AbDataValue local;
    CHECK(ab_settings_statistics_device_sum(test->statistics, span("birdsShooted"),
                                            &total, error) &&
          total == 12 &&
          ab_settings_statistics_get_local(test->statistics, span("birdsShooted"),
                                           &local, error) &&
          local.bits == bits(9) &&
          field(test->notification, "launchImpulse").table == test->vectors + 1 &&
          field(ab_settings_statistics_root(test->statistics), "analyticsSessionId")
                  .kind == AB_DATA_NUMBER &&
          field(ab_settings_statistics_root(test->statistics), "analyticsSessionId")
                  .bits == 0 &&
          field(&test->level.properties->objects[test->bird].physical,
                "shotWithKingSling")
                  .kind == AB_DATA_BOOLEAN &&
          !field(&test->level.properties->objects[test->bird].physical,
                 "shotWithKingSling")
               .bits &&
          field(&test->level.properties->objects[test->bird].physical,
                "shotWithPowerPotion")
                  .kind == AB_DATA_BOOLEAN &&
          !field(&test->level.properties->objects[test->bird].physical,
                 "shotWithPowerPotion")
               .bits);
    uint32_t actual[] = {36 | (body->awake ? 2u : 0u),
                         bits(body->mass.linear_velocity.x),
                         bits(body->mass.linear_velocity.y),
                         bits(body->mass.angular_velocity), bits(body->sleep_time)};
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    AbLevelPropertyObject *property = test->level.properties->objects + test->bird;
    CHECK(
        field(&property->physical, "startImpulseX").bits == data[13] &&
        field(&property->physical, "startImpulseY").bits == data[14] &&
        field(&property->physical, "launchVelocity").table ==
            &property->launch_velocity &&
        field(&property->launch_velocity, "x").bits == bits(bird->launch_velocity.x) &&
        field(&property->launch_velocity, "y").bits == bits(bird->launch_velocity.y) &&
        field(&property->physical, "hasCollided").kind == AB_DATA_BOOLEAN);
    const AbObjectVisual *visual = ab_classic_visuals_object(visuals, test->bird);
    CHECK(visual && visual->sprite.size == strlen("BIRD_RED_FLYING") &&
          !memcmp(visual->sprite.data, "BIRD_RED_FLYING", visual->sprite.size) &&
          field(&property->physical, "sprite").kind == AB_DATA_STRING &&
          field(&property->physical, "sprite").string.data == visual->sprite.data &&
          ab_classic_shots_sync_positions(test->shots, error));
    CHECK(fgetc(native) == '\n' && fgets(marker, sizeof(marker), native) &&
          !strcmp(marker, "AB live bird shot complete\n") && fgetc(native) == EOF &&
          !ferror(native));
    puts("Owned shot adapter matches the retained native impulse/body velocity, "
         "publishes live fields and dispatches both real behavior instances. "
         "Real shared statistics, event delivery and both ordinary shot handlers "
         "and actual decoded audio/mixer are bound. Inactive red-special and "
         "ordinary unpowered attachment paths run against real powerup/physical "
         "storage. No audio device output was opened.");
    return true;
}

static bool encode_effect(AbDataValue value, uint32_t output[2]) {
    output[0] = value.kind == AB_DATA_NUMBER   ? 3u
                : value.kind == AB_DATA_STRING ? 4u
                                               : (uint32_t)value.kind;
    output[1] =
        value.kind == AB_DATA_NIL || value.kind == AB_DATA_STRING ? 0 : value.bits;
    CHECK(value.kind != AB_DATA_STRING || !value.string.size);
    return true;
}

static bool effects_compare(ShotTest *test, FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_CLASSIC_SHOT_EFFECTS_V1 64\n"));
    AbLevelPropertyObject *property = test->level.properties->objects + test->bird;
    AbObjectPropertyAccess raw = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = &property->physical}};
    AbDataValue saved_binding = property->script_entries[1].value;
    for (uint32_t sample = 0; sample < 64; ++sample) {
        uint32_t expected_sample, expected[8], actual[8];
        CHECK(fscanf(file, "%8" SCNx32, &expected_sample) == 1 &&
              expected_sample == sample && read_words(file, expected, 8));
        CHECK(ab_powerup_uses_clear(test->powerups));
        if (sample & 2)
            CHECK(ab_powerup_uses_mark(test->powerups, "KingSling", NULL));
        uint32_t kind = (sample >> 2) & 3;
        AbDataValue potion =
            kind == 0   ? (AbDataValue){0}
            : kind == 1 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
            : kind == 2 ? (AbDataValue){.kind = AB_DATA_NUMBER}
                        : (AbDataValue){.kind = AB_DATA_STRING, .string = span("")};
        CHECK(ab_object_property_set(&raw, span("usedPowerPotion"), potion, NULL) &&
              ab_object_property_set(
                  &raw, span("hasCollided"),
                  (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = (sample & 32) != 0},
                  NULL));
        /* Direct bird fields stay canonical even after a script binding changes. */
        property->script_entries[1].value =
            sample & 32 ? (AbDataValue){0} : saved_binding;
        actual[0] = ab_classic_shot_attach_particles(test->owner, test->bird,
                                                     sample & 1 ? test->powerups : NULL,
                                                     (sample & 1) != 0, NULL);
        actual[1] =
            ab_classic_red_bird_shot(test->owner, test->bird, (sample & 16) != 0, NULL);
        CHECK(encode_effect(field(&property->physical, "shotWithKingSling"),
                            actual + 2) &&
              encode_effect(field(&property->physical, "shotWithPowerPotion"),
                            actual + 4) &&
              encode_effect(field(&property->physical, "hasCollided"), actual + 6));
        CHECK(!memcmp(expected, actual, sizeof(actual)));
        property->script_entries[1].value = saved_binding;
    }
    CHECK(fgetc(file) == '\n' && fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB classic shot effects reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    CHECK(ab_classic_red_bird_shot(NULL, SIZE_MAX, false, NULL));
    puts("Ordinary attachment flags/red-special gates match 64 original cases; "
         "powered spawners and active red-special services deliberately remain "
         "unbound in both references, preserving the observed failure prefixes.");
    return true;
}

#include "classic_sling.h"

int main(int argc, char **argv) {
    if (argc < 3 || argc > 6) {
        fputs("Usage: ab_classic_shots_tests PINNED_APK LIVE_SHOT_CORPUS "
              "[ORDINARY_EFFECT_CORPUS [SLING_WOBBLE_CORPUS [DRAW_GAME_CORPUS]]]\n",
              stderr);
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbClassicLevel *level = NULL;
    AbClassicVisuals *visuals = NULL;
    AbClassicBehaviors *behaviors = NULL;
    AbSpriteAtlas *atlas = NULL;
    AbError error = {0};
    ShotTest test = {0};
    AbRandom48 startup_random = {0};
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat",
                           "assets/data/images/base/INGAME_BLOCKS_2.dat"};
    AbClassicLevelAssets assets = {.random = &startup_random,
                                   .level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = paths,
                                   .sprite_sheet_count = 4,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true,
                                   .derive_placement_scale = true,
                                   .physics_to_world = 20};
    AbWorldDefinition world = {.broadphase_limits = {128, 1024, 1024},
                               .maximum_translation = 2,
                               .maximum_translation_squared = 4,
                               .warm_start = true,
                               .clear_forces = true,
                               .continuous = true};
    FILE *native = fopen(argv[2], "rb");
    FILE *effects = argc >= 4 ? fopen(argv[3], "rb") : NULL;
    FILE *wobble = argc >= 5 ? fopen(argv[4], "rb") : NULL;
    lifecycle_draw_native = argc == 6 ? fopen(argv[5], "rb") : NULL;
    bool okay = native && (argc != 6 || lifecycle_draw_native) &&
                ab_random48_seed(&startup_random, 1) &&
                ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_classic_level_load(
                    &archive, &assets, world,
                    (AbClassicLevelLimits){
                        .objects = 32, .contacts = 128, .storage_bytes = 1024 * 1024},
                    true, &level, &error);
    test.level = ab_classic_level_view(level);
    test.owner = level;
    test.bird = SIZE_MAX;
    for (size_t index = 0; okay && index < test.level.bodies->count; ++index) {
        AbSpan name = test.level.bodies->objects[index].name;
        if (name.size == 9 && !memcmp(name.data, "RedBird_4", 9))
            test.bird = index;
    }
    okay = okay && test.bird != SIZE_MAX && shot_dynamic_registry(&test) &&
           ab_sprite_atlas_open(&archive, test.level.sprites, 16 * 1024 * 1024,
                                8 * 1024 * 1024, &atlas, &error);
    AbSpriteProvider provider = {.context = atlas,
                                 .sprite = removal_sprite,
                                 .composite = NULL,
                                 .texture = removal_texture};
    okay =
        okay &&
        ab_classic_visuals_open(level,
                                (AbClassicVisualResources){.provider = &provider,
                                                           .storage_bytes = 65536,
                                                           .name_capacity = 128},
                                &visuals, &error) &&
        shot_damage_visuals(&test, visuals, &error) &&
        ab_classic_behaviors_open(level,
                                  (AbClassicBehaviorConfig){.flying_bird = &test.flying,
                                                            .context = &test,
                                                            .call = behavior,
                                                            .storage_bytes = 65536},
                                  &behaviors, &error) &&
        launch_check(&test, &archive, level, visuals, behaviors, native, &error);
    if (okay)
        okay = sling_check(&test, behaviors, &error);
    if (okay)
        okay = shot_property_publication(&test, &error);
    if (okay && argc >= 5)
        okay = wobble && sling_wobble_compare(&test, behaviors, wobble, &error);
    if (okay && argc >= 4)
        okay = effects && effects_compare(&test, effects);
    if (okay)
        okay = shot_block_collisions(&test, visuals, behaviors, &error);
    if (okay) {
        /* Release prior shot borrowers before reconstructing behavior groups
         * for the independent collision composition fixture. */
        ab_classic_sling_close(test.sling);
        test.sling = NULL;
        ab_classic_shots_close(test.shots);
        test.shots = NULL;
        ab_classic_behaviors_close(behaviors);
        behaviors = NULL;
        okay = ab_classic_behaviors_open(
                   level,
                   (AbClassicBehaviorConfig){.flying_bird = &test.flying,
                                             .context = &test,
                                             .call = behavior,
                                             .storage_bytes = 65536},
                   &behaviors, &error) &&
               shot_bird_collisions(&test, &archive, visuals, behaviors, &error);
    }
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    ab_audio_mixer_close(test.mixer);
    ab_audio_bank_close(test.audio);
    ab_powerup_uses_close(test.powerups);
    ab_event_manager_close(test.events);
    ab_classic_sling_close(test.sling);
    ab_camera_system_close(test.camera);
    ab_classic_shots_close(test.shots);
    ab_classic_shot_listeners_close(test.game_listeners);
    ab_shot_services_close(test.bindings);
    ab_settings_statistics_close(test.statistics);
    ab_classic_behaviors_close(behaviors);
    ab_classic_visuals_close(visuals);
    ab_sprite_atlas_close(atlas);
    ab_classic_level_close(level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (native && fclose(native))
        okay = false;
    if (effects && fclose(effects))
        okay = false;
    if (wobble && fclose(wobble))
        okay = false;
    if (lifecycle_draw_native && fclose(lifecycle_draw_native))
        okay = false;
    return okay ? 0 : 1;
}
