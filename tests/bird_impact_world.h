#ifndef AB_TEST_BIRD_IMPACT_WORLD_H
#define AB_TEST_BIRD_IMPACT_WORLD_H

#include "ab/bird_impact_world.h"
#include "bird_impact_world_cases.h"

typedef struct {
    uint32_t events[IMPACT_WORLD_MAX_EVENTS][IMPACT_WORLD_EVENT_WORDS];
    uint32_t count, rays, radii, printed;
    float radius;
} ImpactWorldFixture;

static size_t impact_world_matched, impact_world_predictions, impact_world_hits;
static size_t impact_world_rays, impact_world_boundaries;

static uint32_t *impact_world_event(ImpactWorldFixture *fixture, uint32_t kind,
                                    uint32_t ordinal) {
    if (fixture->count >= IMPACT_WORLD_MAX_EVENTS) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    memset(event, 0, IMPACT_WORLD_EVENT_WORDS * sizeof(*event));
    event[0] = kind;
    event[1] = ordinal;
    return event;
}

static bool impact_world_radius(void *context, float *output, AbError *error) {
    (void)error;
    ImpactWorldFixture *fixture = context;
    uint32_t *event = impact_world_event(fixture, 2, ++fixture->radii);
    CHECK(event);
    event[2] = bits(fixture->radius);
    *output = fixture->radius;
    return true;
}

static bool impact_world_ray(void *context, AbVec2 first, AbVec2 second,
                             const AbLevelRaycast *result, AbError *error) {
    (void)error;
    ImpactWorldFixture *fixture = context;
    uint32_t *event = impact_world_event(fixture, 1, ++fixture->rays);
    CHECK(event);
    event[2] = bits(first.x);
    event[3] = bits(first.y);
    event[4] = bits(second.x);
    event[5] = bits(second.y);
    AbLevelRaycast hit = *result;
    event[6] = hit.hit ? 6u : 1u;
    event[7] = hit.hit;
    if (hit.hit) {
        if (hit.name.kind == AB_DATA_STRING) {
            CHECK(hit.name.string.size <= 16);
            event[8] = 1;
            event[9] = (uint32_t)hit.name.string.size;
            game_raycast_pack((const char *)hit.name.string.data, event[9], event + 10);
        } else {
            CHECK(hit.name.kind == AB_DATA_NIL);
        }
        event[14] = bits(hit.point.x);
        event[15] = bits(hit.point.y);
        event[16] = bits(hit.normal.x);
        event[17] = bits(hit.normal.y);
    }
    return true;
}

static bool impact_world_print(void *context, size_t rays, AbError *error) {
    (void)error;
    ImpactWorldFixture *fixture = context;
    CHECK(rays == fixture->rays);
    CHECK(impact_world_event(fixture, 3, (uint32_t)rays));
    fixture->printed = (uint32_t)rays;
    return true;
}

static bool compare_impact_world(FILE *file, GameRayFixture *level, uint32_t scene) {
    for (uint32_t profile = 0; profile < IMPACT_WORLD_CASES; ++profile) {
        uint32_t input[IMPACT_WORLD_INPUT_WORDS];
        bird_impact_world_case(scene, profile, input);
        CHECK(compare_words(file, input, IMPACT_WORLD_INPUT_WORDS, scene, profile));
        ImpactWorldFixture fixture = {.radius = from_bits(input[8])};
        AbBirdImpactInput bird = {
            .position = {from_bits(input[2]), from_bits(input[3])},
            .velocity = {from_bits(input[4]), from_bits(input[5])},
            .gravity = from_bits(input[6]),
            .steps = from_bits(input[7]),
            .steps_present = input[9] != 0};
        AbBirdImpactWorld world = {&level->query.world, &level->bindings,
                                   level->removed};
        AbBirdImpactWorldHost host = {.context = &fixture,
                                      .maximum_raycasts = IMPACT_WORLD_MAX_EVENTS,
                                      .radius = impact_world_radius,
                                      .print = impact_world_print,
                                      .observe = impact_world_ray};
        AbBirdImpactPrediction result = {0};
        AbError error = {0};
        if (!ab_bird_impact_predict_world(&bird, &world, &host, &result, &error)) {
            fprintf(stderr, "Coupled impact prediction failed: %s\n", error.message);
            return false;
        }
        uint32_t output[IMPACT_WORLD_OUTPUT_WORDS] = {0};
        output[1] = result.point_present;
        if (result.point_present) {
            ++impact_world_hits;
            if (result.name.kind == AB_DATA_STRING) {
                CHECK(result.name.string.size <= 16);
                output[2] = 1;
                output[3] = (uint32_t)result.name.string.size;
                game_raycast_pack((const char *)result.name.string.data, output[3],
                                  output + 4);
            } else {
                CHECK(result.name.kind == AB_DATA_NIL);
            }
            output[8] = bits(result.point.x);
            output[9] = bits(result.point.y);
        }
        output[10] = bits(result.time);
        CHECK(result.raycasts == fixture.rays);
        output[11] = fixture.rays;
        output[12] = fixture.radii;
        output[13] = fixture.printed;
        output[14] = fixture.count;
        CHECK(compare_words(file, output, IMPACT_WORLD_OUTPUT_WORDS, scene, profile));
        for (size_t index = 0; index < fixture.count; ++index) {
            CHECK(compare_words(file, fixture.events[index], IMPACT_WORLD_EVENT_WORDS,
                                scene, profile));
        }
        impact_world_matched += IMPACT_WORLD_INPUT_WORDS + IMPACT_WORLD_OUTPUT_WORDS +
                                (size_t)fixture.count * IMPACT_WORLD_EVENT_WORDS;
        impact_world_rays += fixture.rays;
        impact_world_boundaries += fixture.count;
        ++impact_world_predictions;
    }
    return true;
}

#endif
