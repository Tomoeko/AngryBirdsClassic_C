#ifndef AB_TEST_OBJECT_FORCE_STEP_H
#define AB_TEST_OBJECT_FORCE_STEP_H

#include "ab/audio_mixer.h"
#include "ab/object_force_step.h"

typedef struct {
    AbObjectForceInterval *interval;
    AbAudioMixer *mixer;
    AbSpan choices[3];
    size_t sprites, sounds;
} ForceStepNativeHost;

static bool force_step_native_host(void *context, const AbObjectForceCall *call,
                                   AbError *error) {
    ForceStepNativeHost *host = context;
    if (call->effect == AB_FORCE_EFFECT_SPRITE) {
        CHECK(host->interval && host->sprites == 0 && host->sounds == 0);
        CHECK(call->name.data == host->interval->sprite.data &&
              call->name.size == host->interval->sprite.size);
        ++host->sprites;
        return true; /* Original fixture has no attached renderable. */
    }
    CHECK(call->effect == AB_FORCE_EFFECT_AUDIO && host->mixer && host->sprites == 1 &&
          host->sounds == 0);
    ++host->sounds;
    uint32_t identifier = force_sound_id(call->name, host->choices);
    if (!identifier) {
        return true; /* Original Resources returns -1 for a missing clip. */
    }
    static const int16_t silence[2] = {0, 0};
    uint32_t handle;
    return ab_audio_mixer_play(host->mixer, identifier, (AbPcm16View){silence, 2, 1},
                               call->gain, false, 0, &handle, error);
}

static bool force_step_interval_check(const AbObjectForceInterval *state,
                                      const uint32_t expected[4]) {
    const uint32_t values[] = {
        state->timer_present, state->timer_present ? to_bits(state->timer) : 0,
        sprite_code(state->sprite), sprite_code(state->original_sprite)};
    for (size_t field = 0; field < 4; ++field) {
        CHECK(values[field] == expected[field]);
    }
    return true;
}

static bool force_step_native_check(const uint32_t input[36],
                                    const AbBodyMotion *initial,
                                    const AbBodyMotion *expected_body,
                                    const AbObjectForces *settings, bool interval,
                                    const uint32_t interval_expected[4],
                                    const ForceSoundSnapshot *audio) {
    AbObjectForceInterval state = {
        .interval = from_bits(input[25]),
        .timer = from_bits(input[26]),
        .timer_present = (input[27] & 1) != 0,
        .original_present = (input[27] & 2) != 0,
        .sprite = sprite_span("rest"),
        .applied_sprite = sprite_span("active"),
        .original_sprite = input[27] & 2 ? sprite_span("saved") : (AbSpan){0},
    };
    ForceStepNativeHost host = {
        .interval = interval ? &state : NULL,
        .choices = {sprite_span("sound1"), sprite_span("sound2"),
                    sprite_span("sound3")},
    };
    AbObjectForceSound sound = {
        .kind = input[30] == 2                     ? AB_FORCE_SOUND_ARRAY
                : input[30] == 1 || input[30] == 4 ? AB_FORCE_SOUND_NAMED
                                                   : AB_FORCE_SOUND_NONE,
        .name = audio
                    ? (input[30] == 4 ? sprite_span("1") : host.choices[input[35] - 1])
                    : (AbSpan){0},
        .choices = host.choices,
        .count = input[31],
        .gain = from_bits(input[33]),
        .gain_present = input[34] != 0,
    };
    AbRandom48 random;
    CHECK(ab_random48_seed(&random, input[32]));
    AbError error = {0};
    if (audio) {
        CHECK(ab_audio_mixer_open((AbAudioMixerLimits){1, 2, 1, 4096}, &host.mixer,
                                  &error));
    }
    AbObjectForceStep step = {.forces = settings,
                              .gravity = {from_bits(input[7]), from_bits(input[8])},
                              .interval = interval ? &state : NULL,
                              .sound = audio ? &sound : NULL,
                              .random = audio ? &random : NULL,
                              .host = force_step_native_host,
                              .context = &host};
    AbBodyMotion body = *initial;
    bool okay = (!audio || ab_audio_mixer_start(host.mixer, true, &error)) &&
                ab_object_force_step(&body, &step, from_bits(input[21]), &error) &&
                (!interval || force_step_interval_check(&state, interval_expected));
    uint32_t after;
    if (okay && audio) {
        okay = ab_random48_next(&random, &after) && after == audio->summary[0] &&
               force_sound_voice_check(host.mixer, audio->voice, audio->summary);
    }
    ab_audio_mixer_close(host.mixer);
    CHECK(okay);
    CHECK(memcmp(&body, expected_body, sizeof(body)) == 0);
    return true;
}

typedef struct {
    AbBodyMotion *body;
    AbObjectForces *forces;
    AbObjectForceInterval *interval;
    AbObjectForceSound *sound;
    AbObjectForceEffect fail;
    size_t count;
} ForceStepEdgeHost;

static bool force_step_edge_host(void *context, const AbObjectForceCall *call,
                                 AbError *error) {
    (void)error;
    ForceStepEdgeHost *host = context;
    CHECK(host->body->awake && host->body->force.y == -20 &&
          host->body->mass.linear_velocity.x == 0);
    if (call->effect == AB_FORCE_EFFECT_SPRITE) {
        CHECK(host->count == 0 && host->interval->timer == 0 &&
              host->interval->timer_present);
        CHECK(sprite_code(host->interval->sprite) == 2 &&
              host->interval->original_present &&
              sprite_code(host->interval->original_sprite) == 1);
        host->forces->force.x = 4;
        host->forces->disabled = true;
        host->sound->gain = 0.25f;
        host->sound->gain_present = true;
        host->interval->timer = 77;
    } else {
        CHECK(call->effect == AB_FORCE_EFFECT_AUDIO && host->count == 1 &&
              host->interval->timer == 77 && call->gain == 0.25f);
        host->forces->object_mass = 3;
    }
    ++host->count;
    return host->fail != call->effect;
}

static bool force_step_edge_checks(void) {
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        AbBodyMotion body = {
            .mass = {.type = AB_BODY_DYNAMIC, .mass = 2, .inverse_mass = 0.5f},
            .sleep_time = 7};
        AbObjectForces forces = {
            .force = {2, 0}, .object_mass = 1, .force_x_present = true};
        AbObjectForceInterval interval = {.interval = 1,
                                          .timer_present = true,
                                          .sprite = sprite_span("rest"),
                                          .applied_sprite = sprite_span("active")};
        AbObjectForceSound sound = {.kind = AB_FORCE_SOUND_NAMED,
                                    .name = sprite_span("sound1")};
        ForceStepEdgeHost host = {.body = &body,
                                  .forces = &forces,
                                  .interval = &interval,
                                  .sound = &sound,
                                  .fail = scenario == 1   ? AB_FORCE_EFFECT_SPRITE
                                          : scenario == 2 ? AB_FORCE_EFFECT_AUDIO
                                                          : 0};
        AbObjectForceStep step = {.forces = &forces,
                                  .gravity = {0, 10},
                                  .interval = scenario == 5 ? NULL : &interval,
                                  .sound = &sound,
                                  .host = scenario == 4 ? NULL : force_step_edge_host,
                                  .context = &host};
        if (scenario == 3) {
            forces.disabled = true;
            interval.interval = NAN;
            interval.timer = NAN;
            sound.gain = NAN;
            sound.gain_present = true;
        }
        AbError error = {0};
        bool okay = ab_object_force_step(&body, &step, scenario == 5 ? NAN : 0, &error);
        CHECK(okay == (scenario == 0 || scenario == 3 || scenario == 5));
        CHECK(body.awake && body.sleep_time == 0 && body.force.y == -20);
        if (scenario == 0) {
            CHECK(host.count == 2 && body.mass.linear_velocity.x == 6 &&
                  body.force.x == 0 && interval.timer == 1);
        } else if (scenario == 1 || scenario == 2) {
            CHECK(host.count == scenario && body.mass.linear_velocity.x == 0 &&
                  interval.timer == 77);
        } else if (scenario == 3) {
            CHECK(host.count == 0 && isnan(interval.timer) &&
                  sprite_code(interval.sprite) == 1);
        } else if (scenario == 4) {
            CHECK(host.count == 0 && interval.timer == 0 &&
                  sprite_code(interval.sprite) == 2);
        } else {
            CHECK(host.count == 0 && body.force.x == 2 &&
                  body.mass.linear_velocity.x == 0);
        }
    }
    return true;
}

#endif
