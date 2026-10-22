#ifndef AB_TEST_CLASSIC_SLING_H
#define AB_TEST_CLASSIC_SLING_H

#include "sling_wobble_cases.h"

/* Full C integration with explicit initial controller and camera inputs. Native
 * fidelity of the arithmetic components is checked by their separate retained
 * corpora; this fixture does not claim an original end-to-end frame replay. */
static bool sling_camera_distances(void *context, float *range, float *distance,
                                   AbError *error) {
    ShotTest *test = context;
    return ab_classic_launch_host_camera_distances(&test->launch_host, range, distance,
                                                   error);
}

static bool sling_reload_sound(void *context, float gain, AbError *error) {
    ShotTest *test = context;
    CHECK(gain == 1 &&
          ab_classic_launch_host_reload_audio(&test->launch_host, gain, error));
    ++test->reload_sounds;
    return true;
}

static bool sling_find_current(ShotTest *test, size_t *output) {
    AbShotState *shot = ab_classic_shots_state(test->shots);
    for (size_t index = 0; index < test->level.bodies->count; ++index) {
        AbShotBird *bird = ab_classic_shots_bird(test->shots, index);
        if (bird && bird->name && shot->current_bird_name &&
            !strcmp(bird->name, shot->current_bird_name)) {
            *output = index;
            return true;
        }
    }
    CHECK(false);
}

static bool sling_advance_queue(ShotTest *test, AbError *error) {
    AbShotState *shot = ab_classic_shots_state(test->shots);
    for (size_t frame = 0; frame < 8 && !shot->bird_ready; ++frame)
        CHECK(ab_classic_sling_update(test->sling, 0.25f, error));
    CHECK(shot->bird_ready && !shot->bird_fired &&
          !ab_classic_sling_view(test->sling).fill_in_next_bird &&
          ab_classic_sling_view(test->sling).transfer_object == SIZE_MAX &&
          !ab_classic_sling_view(test->sling).launch_cue);
    CHECK(sling_find_current(test, &test->bird));
    AbShotBird *bird = ab_classic_shots_bird(test->shots, test->bird);
    CHECK(bird->position.x == test->rest.x - 0.1f &&
          bird->position.y == test->rest.y - 0.1f &&
          test->level.character_objects[test->bird].jump.timer == 10000);
    size_t body = test->level.bodies->objects[test->bird].physics.body;
    CHECK(test->level.world->storage.bodies[body].motion.mass.type == AB_BODY_DYNAMIC);
    return true;
}

static bool sling_mouse_shot(ShotTest *test, AbError *error) {
    AbShotState *shot = ab_classic_shots_state(test->shots);
    AbSlingMouseInput input = {.cursor = {100, 100},
                               .cursor_physics = test->rest,
                               .seconds = 1.0f / 60.0f,
                               .touch_count = 1,
                               .screen_width = 480,
                               .zoom = 1,
                               .pressed = true,
                               .held = true,
                               .sling_disabled = true,
                               .sling_visible = true};
    AbCameraValues queued = {.position = {200, 50}, .present = 3};
    CHECK(ab_camera_system_goto_position(test->camera, &queued, NULL, error) &&
          ab_camera_system_view(test->camera).automatic_present &&
          ab_classic_sling_mouse(test->sling, input, error) && shot->dragged &&
          !ab_camera_system_view(test->camera).automatic_present &&
          !ab_camera_system_view(test->camera).mover->target_position_present &&
          !ab_camera_system_view(test->camera).mover->target_zoom_present);
    input.pressed = false;
    input.cursor.x = 80;
    input.cursor.y = 110;
    input.cursor_physics = (AbVec2){test->rest.x - 4, test->rest.y + 2};
    CHECK(ab_classic_sling_mouse(test->sling, input, error) &&
          ab_classic_sling_view(test->sling).band.distance > 2 &&
          ab_classic_sling_update(test->sling, 0, error));
    AbVec2 band = ab_classic_sling_view(test->sling).band.rubber_band;
    CHECK(shot->dragged->position.x == band.x && shot->dragged->position.y == band.y);
    ab_classic_shot_listeners_state(test->game_listeners)->current_bird_index =
        ab_classic_sling_view(test->sling).current_bird_index;
    input.held = false;
    input.released = true;
    CHECK(ab_classic_sling_mouse(test->sling, input, error) && !shot->dragged &&
          !shot->bird_ready && shot->bird_fired &&
          ab_classic_sling_view(test->sling).launch_cue);
    return true;
}

static bool fresh_sling_state(ShotTest *test, AbClassicSlingConfig config,
                              AbError *error) {
    AbClassicSling *sling = NULL;
    AbShotState *shot = ab_classic_shots_state(config.shots);
    AbShotState saved = *shot;
    CHECK(ab_classic_sling_open(config, &sling, error));
    shot->dragged = ab_classic_shots_bird(config.shots, test->bird);
    shot->selection_played = shot->bird_fired = shot->bird_ready = true;
    bool laser = false;
    CHECK(ab_audio_bank_is_playing(test->audio, span("laser_sight"), &laser, error) &&
          ab_classic_sling_disable_for_level(sling, error) &&
          ab_classic_sling_level_reset(sling, error));
    AbClassicSlingView reset = ab_classic_sling_view(sling);
    CHECK(!reset.disabled_for_current_level && !reset.fill_in_next_bird &&
          reset.allow_reset_to_bird_camera == config.allow_reset_to_bird_camera &&
          reset.current_bird_index == config.current_bird_index &&
          reset.transfer_object == SIZE_MAX && reset.transfer.time == 0 &&
          reset.transfer.phase_angle == 0.75f * 0x1.921fb6p+1f &&
          reset.band.rest.x == 0 && reset.band.rest.y == 0 &&
          reset.band.rubber_band.x == 0 && reset.band.rubber_band.y == 0 &&
          reset.band.angle == 0 && reset.band.distance == 0 &&
          reset.band.scope_sound_eligible == !laser && !shot->dragged &&
          !shot->selection_played && !shot->bird_fired && !shot->bird_ready &&
          shot->bird_reload_delay == 1 && shot->next_bird_timer == 0.5f);
    AbDataValue x, y, radius;
    CHECK(ab_level_property_get(test->level.properties, test->bird, span("x"), &x,
                                error) &&
          ab_level_property_get(test->level.properties, test->bird, span("y"), &y,
                                error) &&
          ab_level_property_get(test->level.properties, test->bird, span("radius"),
                                &radius, error));
    AbSlingshotStartInput input = {
        .object_position = {from_bits(x.bits), from_bits(y.bits)},
        .radius = from_bits(radius.bits),
        .back_pivot_y = 9,
        .back_height = 100,
        .physics_scale = config.physics_scale,
        .object_present = true,
        .radius_present = true};
    AbSlingshotStartPose expected;
    CHECK(ab_slingshot_start_pose(&input, &expected, error) &&
          ab_classic_sling_position_start(sling, test->bird, 9, 100, error));
    AbSlingshotStartPose actual = ab_classic_sling_start_pose(sling);
    CHECK(actual.base.x == input.object_position.x &&
          actual.base.y == input.object_position.y &&
          actual.level_start.y == expected.level_start.y &&
          actual.band.x == expected.band.x && actual.band.y == expected.band.y &&
          ab_classic_sling_view(sling).band.rest.y == expected.level_start.y &&
          ab_classic_sling_view(sling).band.rubber_band.y == expected.band.y &&
          shot->current_bird_name == saved.current_bird_name &&
          shot->flying == saved.flying && shot->birds_shot == saved.birds_shot &&
          !shot->bird_ready &&
          ab_classic_sling_position_start(sling, SIZE_MAX, NAN, NAN, error) &&
          ab_classic_sling_start_pose(sling).band.y == expected.band.y);
    ab_classic_sling_close(sling);
    *shot = saved;
    return true;
}

static bool sling_check(ShotTest *test, AbClassicBehaviors *behaviors, AbError *error) {
    AbClassicSling *invalid = (AbClassicSling *)(uintptr_t)1;
    CHECK(!ab_classic_sling_open((AbClassicSlingConfig){0}, &invalid, NULL) &&
          !invalid && !ab_classic_sling_bytes(NULL) &&
          !ab_classic_sling_update(NULL, 0, NULL) &&
          !ab_classic_sling_stretch(NULL, (AbVec2){0}, NULL));
    test->launch_powerups_enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    test->launch_next_sound =
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("bird_next_military")};
    CHECK(ab_classic_launch_host_initialize(
        (AbClassicLaunchHostConfig){.level = test->owner,
                                    .shots = &test->shots,
                                    .sling = &test->sling,
                                    .audio = test->audio,
                                    .camera = &test->camera,
                                    .next_bird_sound = &test->launch_next_sound,
                                    .powerups = test->powerups,
                                    .powerups_enabled = &test->launch_powerups_enabled,
                                    .red_special_available = &test->launch_red_special},
        &test->launch_host, error));
    AbClassicSlingConfig config = {
        .level = test->owner,
        .shots = test->shots,
        .behaviors = behaviors,
        .services = test->bindings,
        .events = test->events,
        .audio = test->audio,
        .camera = test->camera,
        .start = {.level_start = test->rest, .band = test->band},
        .physics_scale = test->physics_scale,
        .default_force = test->default_force,
        .current_bird_index = 1,
        .allow_reset_to_bird_camera = true,
        .context = test,
        .camera_distances = sling_camera_distances,
        .reload_sound = sling_reload_sound,
        .storage_bytes = 16384};
    CHECK(fresh_sling_state(test, config, error) &&
          ab_classic_sling_open(config, &test->sling, error) &&
          ab_classic_sling_bytes(test->sling) <= config.storage_bytes &&
          sling_advance_queue(test, error));
    AbShotState *shot = ab_classic_shots_state(test->shots);
    CHECK(ab_classic_sling_select(test->sling, error) &&
          ab_classic_sling_stretch(test->sling, (AbVec2){0.5f, 0}, error) &&
          ab_classic_sling_release(test->sling, error) && !shot->dragged &&
          shot->bird_ready && shot->birds_shot == 1 &&
          ab_classic_sling_view(test->sling).band.distance == 0 &&
          ab_classic_sling_view(test->sling).band.scope_sound_eligible &&
          ab_classic_sling_update(test->sling, 0, error));
    CHECK(sling_mouse_shot(test, error) && shot->birds_shot == 2);
    const AbDataTable *second_event = test->notification;
    const AbDataTable *second_position = field(second_event, "launchPosition").table;
    const AbDataTable *second_impulse = field(second_event, "launchImpulse").table;
    uint32_t impulse_x = field(second_impulse, "x").bits;
    CHECK(second_position && second_impulse && sling_advance_queue(test, error) &&
          field(second_position, "x").bits == bits(test->rest.x - 0.1f) &&
          field(second_position, "y").bits == bits(test->rest.y - 0.1f) &&
          sling_mouse_shot(test, error) && shot->birds_shot == 3 &&
          field(test->notification, "launchPosition").table != second_position &&
          field(test->notification, "launchImpulse").table != second_impulse &&
          field(second_impulse, "x").bits == impulse_x &&
          field(second_position, "x").bits == bits(test->rest.x - 0.1f) &&
          test->reload_sounds == 2 && test->notification_calls == 3 &&
          ab_shot_services_notification_count(test->bindings) == 4);
    CHECK(ab_classic_sling_update(test->sling, 0.5f, error) &&
          !ab_classic_sling_view(test->sling).allow_reset_to_bird_camera &&
          !shot->bird_ready && shot->bird_fired);
    float total;
    CHECK(ab_settings_statistics_device_sum(test->statistics, span("birdsShooted"),
                                            &total, error) &&
          total == 14);
    AbClassicLaunchHost edge = {.failed = true};
    CHECK(!ab_classic_launch_host_initialize((AbClassicLaunchHostConfig){0}, &edge,
                                             NULL) &&
          edge.failed);
    edge = test->launch_host;
    AbShotCall call = {.operation = AB_SHOT_DEFINITION_HOOK, .text = "redBirdShot"};
    CHECK(ab_classic_launch_host_remaining(&edge, &call, error) && !edge.failed);
    call.text = "unimplementedCatalogHook";
    AbError missing = {0};
    CHECK(!ab_classic_launch_host_remaining(&edge, &call, &missing) &&
          strstr(missing.message, "Missing reached actual catalog shot hook") &&
          edge.failed && !ab_classic_launch_host_remaining(&edge, &call, NULL));
    edge = test->launch_host;
    call.operation = AB_SHOT_LEVEL_HOOK;
    CHECK(!ab_classic_launch_host_remaining(&edge, &call, &missing) &&
          strstr(missing.message, "Missing reached actual level shot hook"));
    edge = test->launch_host;
    call = (AbShotCall){.operation = AB_SHOT_PLAY_AUDIO, .value = 1};
    size_t pending_audio = ab_audio_mixer_view(test->mixer).pending_count;
    CHECK(ab_classic_launch_host_remaining(&edge, &call, error) && !edge.failed &&
          ab_audio_mixer_view(test->mixer).pending_count == pending_audio);
    edge = test->launch_host;
    AbClassicSling *unbound = NULL;
    edge.config.sling = &unbound;
    CHECK(!ab_classic_launch_host_dispatch(&edge, &call, &missing) &&
          strstr(missing.message, "Missing shared actual launch shot/sling owners") &&
          !edge.failed);
    puts("Owned sling reloads both remaining birds and drives two further shots "
         "through mouse input, real bodies, statistics, listeners and audio. "
         "Cancel, held motion and retained event/vector aliases pass. Initial "
         "controller/camera values are fixture inputs; no complete game frame "
         "or native end-to-end parity is claimed.");
    return true;
}

static bool sling_wobble_compare(ShotTest *test, AbClassicBehaviors *behaviors,
                                 FILE *native, AbError *error) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), native) &&
          !strcmp(marker, "AB_ANDROID_SLING_WOBBLE_V1 64\n"));
    AbShotState *shot = ab_classic_shots_state(test->shots);
    AbShotState saved = *shot;
    AbClassicSling *sling = NULL;
    for (unsigned sample = 0; sample < SLING_WOBBLE_CASES; ++sample) {
        int16_t pcm[512];
        CHECK(ab_audio_mixer_mix(test->mixer, 512, pcm, 512, error) &&
              ab_audio_mixer_all_action(test->mixer, AB_AUDIO_STOP, error) &&
              ab_audio_mixer_mix(test->mixer, 512, pcm, 512, error));
        uint32_t index, inputs[3], expected[6];
        float input[3];
        sling_wobble_input(sample, input);
        CHECK(read_words(native, &index, 1) && index == sample &&
              read_words(native, inputs, 3) && read_words(native, expected, 6));
        for (size_t field = 0; field < 3; ++field)
            CHECK(inputs[field] == bits(input[field]));
        AbClassicSlingConfig config = {
            .level = test->owner,
            .shots = test->shots,
            .behaviors = behaviors,
            .services = test->bindings,
            .events = test->events,
            .audio = test->audio,
            .start = {.level_start = {12, 6.2f}, .band = {12, 6.2f}},
            .physics_scale = 0.05f,
            .current_bird_index = 1,
            .storage_bytes = 16384};
        CHECK(ab_classic_sling_open(config, &sling, error));
        shot->dragged = ab_classic_shots_bird(test->shots, test->bird);
        shot->selection_played = false;
        shot->bird_fired = false;
        CHECK(ab_classic_sling_stretch(sling, (AbVec2){input[0], input[1]}, error));
        shot->bird_fired = true;
        CHECK(ab_classic_sling_wobble(sling, input[2], 16, error));
        AbSlingBandState band = ab_classic_sling_view(sling).band;
        uint32_t actual[] = {bits(band.rubber_band.x), bits(band.rubber_band.y),
                             bits(band.angle),         bits(band.distance),
                             bits(band.wobble),        bits(band.previous_distance)};
        for (size_t field = 0; field < 6; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Sling spring case %u field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        sample, field, actual[field], expected[field]);
                ab_classic_sling_close(sling);
                return false;
            }
        }
        shot->bird_fired = false;
        CHECK(ab_classic_sling_wobble(sling, NAN, 0, error));
        shot->bird_fired = true;
        CHECK(!ab_classic_sling_wobble(sling, 0.1f, 0, NULL));
        ab_classic_sling_close(sling);
        sling = NULL;
    }
    *shot = saved;
    CHECK(fgetc(native) == '\n' && fgets(marker, sizeof(marker), native) &&
          !strcmp(marker, "AB sling wobble reference complete\n") &&
          fgetc(native) == EOF && !ferror(native));
    puts("Owned sling spring matches 64 original full-method calls after actual "
         "stretch/reset setup, including zero distance and the 0.05-second edge.");
    return true;
}

static bool shot_property_publication(ShotTest *test, AbError *error) {
    AbClassicPostPhysics *post = NULL;
    AbClassicPostConfig config = {.minimum_x = -1000,
                                  .maximum_x = 1000,
                                  .audio = ab_classic_audio_bank_rolling,
                                  .context = test->audio,
                                  .storage_bytes = 65536};
    CHECK(ab_classic_post_physics_open(test->owner, config, &post, error));
    bool okay = ab_classic_post_physics_update(post, true, error);
    if (okay) {
        const AbDataTable *physical =
            &test->level.properties->objects[test->bird].physical;
        const AbClassicMotionProperties *motion =
            test->level.motion_properties + test->bird;
        okay =
            motion->initialized && field(physical, "xVel").kind == AB_DATA_NUMBER &&
            field(physical, "xVel").bits == bits(motion->velocity.x) &&
            field(physical, "yVel").bits == bits(motion->velocity.y) &&
            field(physical, "angle").bits ==
                bits(test->level.character_objects[test->bird].jump.rotation) &&
            field(physical, "sleeping").kind == AB_DATA_BOOLEAN &&
            field(physical, "sleeping").bits == motion->sleeping &&
            field(physical, "outsideBoundaries").bits == motion->outside_boundaries &&
            field(physical, "launchVelocity").kind == AB_DATA_TABLE &&
            field(physical, "sprite").kind == AB_DATA_STRING;
    }
    ab_classic_post_physics_close(post);
    CHECK(okay);
    puts("Real post-physics publication shares shot/sprite fields and canonical "
         "motion properties; rolling requests use the shared audio bank.");
    return true;
}

#endif
