#ifndef AB_TEST_CLASSIC_GAME_LEVEL_PARTICLES_H
#define AB_TEST_CLASSIC_GAME_LEVEL_PARTICLES_H

#include "ab/classic_game_level_particles.h"
#include "ab/classic_particles.h"
#include "ab/particle_draw.h"

typedef struct {
    AbClassicGameLevelParticles *owner;
    AbParticles *engine;
    size_t expected, extra_calls;
} LifecycleLevelParticles;

static bool lifecycle_level_extra(void *context, float seconds, float time,
                                  AbError *error) {
    LifecycleLevelParticles *test = context;
    (void)seconds;
    (void)error;
    CHECK(time == 4 && ab_particles_level_count(test->engine) == test->expected &&
          !ab_classic_game_level_particles_update(test->owner, 0, time, NULL));
    ++test->extra_calls;
    return true;
}

/* Explicit emission metadata probes source scheduling with actual loaded
 * definitions, sprite bindings and engine RNG. It is not Level1's resolved
 * background-particle bootstrap or a full original/native frame. */
static bool lifecycle_level_particles(AbClassicParticles *particles,
                                      const AbSpriteProvider *provider,
                                      AbError *error) {
    AbParticles *engine = ab_classic_particles_engine(particles);
    CHECK(!ab_particles_level_count(engine));
    size_t normal = ab_particles_count(engine, false);
    AbDataEntry rate = {{.kind = AB_DATA_STRING, .string = span("amount")},
                        {.kind = AB_DATA_NUMBER, .bits = bits(0.1f)}};
    AbDataTable settings = {&rate, 1, 1, NULL};
    AbDataEntry fields[] = {
        {{.kind = AB_DATA_STRING, .string = span("particles")},
         {.kind = AB_DATA_STRING, .string = span("redBuff")}},
        {{.kind = AB_DATA_STRING, .string = span("firstFrame")},
         {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {{.kind = AB_DATA_STRING, .string = span("settingsFrame")},
         {.kind = AB_DATA_TABLE, .table = &settings}},
        {{.kind = AB_DATA_STRING, .string = span("x")},
         {.kind = AB_DATA_NUMBER, .bits = bits(10)}},
        {{.kind = AB_DATA_STRING, .string = span("y")},
         {.kind = AB_DATA_NUMBER, .bits = bits(-60)}},
        {{.kind = AB_DATA_STRING, .string = span("width")},
         {.kind = AB_DATA_NUMBER, .bits = bits(4)}},
        {{.kind = AB_DATA_STRING, .string = span("height")}, {.kind = AB_DATA_BOOLEAN}},
        {{.kind = AB_DATA_STRING, .string = span("startAtGroundLevel")},
         {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataTable source = {fields, 8, 8, NULL};
    AbDataValue level = {.kind = AB_DATA_TABLE, .table = &source};
    AbDataValue enabled = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    float timer = 0;
    bool physics = true;
    AbClassicGameLevelParticles owner = {0};
    LifecycleLevelParticles test = {&owner, engine, 2, 0};
    CHECK(ab_classic_game_level_particles_initialize(
        (AbClassicGameLevelParticleConfig){.particles = engine,
                                           .level_particles = &level,
                                           .enabled = &enabled,
                                           .physics_enabled = &physics,
                                           .timer = &timer,
                                           .context = &test,
                                           .extra_update = lifecycle_level_extra},
        &owner, error));
    CHECK(ab_classic_game_level_particles_update(&owner, NAN, 4, error) &&
          fields[1].value.kind == AB_DATA_BOOLEAN && !fields[1].value.bits &&
          timer == 0 && test.extra_calls == 1);
    const AbParticle *first = ab_particles_level_at(engine, 0);
    const AbParticle *second = ab_particles_level_at(engine, 1);
    CHECK(first && second && first->position.y == -20 && second->position.y == -20 &&
          first->layer == second->layer && (first->layer == 1 || first->layer == 2) &&
          ab_classic_game_level_particles_update(&owner, 10, 4, error) && timer == 10);
    physics = false;
    CHECK(ab_classic_game_level_particles_update(&owner, 0.125f, 4, error) &&
          timer == 10.125f);
    physics = true;
    test.expected = 3;
    CHECK(ab_classic_game_level_particles_update(&owner, 0, 4, error) && timer == 0);
    enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_classic_game_level_particles_update(&owner, NAN, 4, error) && timer == 0);
    enabled.bits = 1;
    fields[1].value.bits = 1;
    fields[7].value.bits = 0;
    test.expected = 5;
    CHECK(ab_classic_game_level_particles_update(&owner, 0, 4, error) &&
          !fields[1].value.bits);
    rate.value.bits = bits(0);
    CHECK(ab_classic_game_level_particles_update(&owner, 1, 4, error) && timer == 1);
    rate.value.bits = bits(-0.0f);
    test.expected = 6;
    CHECK(ab_classic_game_level_particles_update(&owner, 0, 4, error) && timer == 0 &&
          test.extra_calls == 8 && ab_particles_count(engine, false) == normal);
    AbDrawCommand commands[16];
    AbDrawList list = {commands, 0, 16};
    AbParticleDrawStyle style = {.provider = provider,
                                 .camera = {{-100, -100}, {1, 1}},
                                 .width = 480,
                                 .height = 320,
                                 .blend = AB_BLEND_PREMULTIPLIED,
                                 .color = {1, 1, 1, 1},
                                 .opacity = 1};
    CHECK(ab_particles_draw_level(engine, style, 0, &list, error) && list.count == 6);
    size_t selected = 0;
    for (int32_t layer_index = 1; layer_index <= 2; ++layer_index) {
        list.count = 0;
        CHECK(ab_particles_draw_level(engine, style, layer_index, &list, error));
        selected += list.count;
    }
    CHECK(selected == 6);
    fields[0].value.string = span("unresolvedLevelParticle");
    fields[1].value.bits = 1;
    rate.value.bits = bits(0.1f);
    AbError late = {0};
    CHECK(!ab_classic_game_level_particles_update(&owner, 0, 4, &late) &&
          strstr(late.message, "Unknown particle definition") && fields[1].value.bits &&
          ab_particles_level_count(engine) == 6 && test.extra_calls == 8 &&
          owner.failed && !owner.updating &&
          !ab_classic_game_level_particles_update(&owner, 0, 4, NULL) &&
          ab_particles_level_count(engine) == 6);
    return true;
}

#endif
