#include "ab/floating_scores.h"
#include "ab/score.h"
#include "ab/string_map.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Floating score check at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool removal_storage(void) {
    AbFloatingScores *scores = NULL;
    CHECK(ab_floating_scores_open((AbFloatingScoreLimits){2, 8, 4096}, &scores, NULL));
    uint8_t sprite[] = "5K_GREEN", text[] = "20";
    AbBlockRemovalFloatingScore request = {
        .x = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40200000)},
        .y = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0xc0400000)},
        .sprite = {.kind = AB_DATA_STRING, .string = {sprite, 8}},
        .score = {.present = true, .value = 5000},
        .lifetime = 0.9f,
        .max_scale = 2};
    AbBlockRemovalFloatingScore absent = {.text = {NULL, SIZE_MAX}};
    CHECK(ab_floating_scores_add_removal(scores, &absent, NULL) &&
          !ab_floating_scores_count(scores) &&
          ab_floating_scores_add_removal(scores, &request, NULL));
    AbBirdCollisionFloatingScore *first = ab_floating_scores_at(scores, 0);
    sprite[0] = 'X';
    CHECK(first && first->position.x == 2.5f && first->position.y == -3 &&
          first->score == 5000 && first->lifetime == 0.9f && !first->text_present &&
          !first->text.data && !first->text.size &&
          first->sprite.kind == AB_DATA_STRING && first->sprite.string.size == 8 &&
          first->sprite.string.data != sprite &&
          !memcmp(first->sprite.string.data, "5K_GREEN", 8) &&
          ab_floating_scores_update(scores, 0.25f, NULL) && first->xs == 2);
    request.score.value = 20;
    request.text_present = true;
    request.text = (AbSpan){text, 2};
    /* Both strings together exceed this record's eight-byte bound. */
    CHECK(!ab_floating_scores_add_removal(scores, &request, NULL) &&
          ab_floating_scores_count(scores) == 1);
    request.sprite = (AbDataValue){0};
    request.lifetime = 0.6f;
    CHECK(ab_floating_scores_add_removal(scores, &request, NULL));
    AbBirdCollisionFloatingScore *second = ab_floating_scores_at(scores, 1);
    text[0] = '9';
    CHECK(second && second->text_present && second->text.size == 2 &&
          !memcmp(second->text.data, "20", 2) && second->sprite.kind == AB_DATA_NIL &&
          !memcmp(first->sprite.string.data, "5K_GREEN", 8));
    ab_floating_scores_close(scores);
    return true;
}

static bool storage_and_timing(void) {
    AbFloatingScores *scores = NULL;
    CHECK(!ab_floating_scores_open((AbFloatingScoreLimits){3, 8, 1}, &scores, NULL) &&
          !scores &&
          ab_floating_scores_open((AbFloatingScoreLimits){3, 8, 4096}, &scores, NULL) &&
          ab_floating_scores_bytes(scores) <= 4096 &&
          ab_floating_scores_update(scores, NAN, NULL));
    uint8_t text[] = {'1', '0'};
    AbBirdCollisionFloatingScore request = {.position = {2, 3},
                                            .text = {text, sizeof(text)},
                                            .text_present = true,
                                            .score = 10,
                                            .lifetime = 0.5f,
                                            .max_scale = 2};
    AbBirdCollisionFloatingScore unused = {.score = NAN, .time = INFINITY};
    CHECK(ab_floating_scores_add(scores, &unused, NULL) &&
          !ab_floating_scores_used(scores) &&
          ab_floating_scores_add(scores, &request, NULL));
    AbBirdCollisionFloatingScore *first = ab_floating_scores_at(scores, 0);
    request.lifetime = 0.25f;
    CHECK(ab_floating_scores_add(scores, &request, NULL));
    AbBirdCollisionFloatingScore *second = ab_floating_scores_at(scores, 1);
    text[0] = '9';
    CHECK(first && second && first != second && first->text.data != text &&
          first->text.size == 2 && !memcmp(first->text.data, "10", 2) &&
          !memcmp(second->text.data, "10", 2) &&
          ab_floating_scores_update(scores, 0.125f, NULL) && first->time == 0.125f &&
          first->xs == 1 && second->xs == 1 &&
          ab_floating_scores_update(scores, 0.125f, NULL) && first->xs == 2 &&
          second->xs == 0 && ab_floating_scores_count(scores) == 2 &&
          ab_floating_scores_update(scores, 0.125f, NULL) && first->xs == 1 &&
          second->xs == -1 && second->time == 0.375f &&
          ab_floating_scores_count(scores) == 1 &&
          ab_floating_scores_at(scores, 0) == first &&
          ab_floating_scores_update(scores, 0.125f, NULL) && first->xs == 0 &&
          ab_floating_scores_count(scores) == 1 &&
          ab_floating_scores_update(scores, 0, NULL) &&
          ab_floating_scores_count(scores) == 1 &&
          ab_floating_scores_update(scores, 0.125f, NULL) && first->xs == -1 &&
          !ab_floating_scores_count(scores) && ab_floating_scores_used(scores) == 2 &&
          !memcmp(first->text.data, "10", 2) && !memcmp(second->text.data, "10", 2));
    request.lifetime = 0.125f;
    CHECK(ab_floating_scores_add(scores, &request, NULL));
    AbBirdCollisionFloatingScore *third = ab_floating_scores_at(scores, 0);
    CHECK(third && third != first && third != second &&
          !memcmp(third->text.data, "90", 2) &&
          !ab_floating_scores_add(scores, &request, NULL) &&
          ab_floating_scores_count(scores) == 1 &&
          ab_floating_scores_used(scores) == 3 &&
          ab_floating_scores_add(scores, &unused, NULL) &&
          ab_floating_scores_update(scores, -0.125f, NULL) && third->time == -0.125f &&
          third->xs == -1);
    ab_floating_scores_close(scores);
    return true;
}

typedef struct {
    AbFloatingScores *floating;
    AbStringMap *hats;
    AbBirdCollisionScoreEntry score;
} PreludeFixture;

static bool hat_lookup(void *context, AbSpan name, AbBirdCollisionHat **output,
                       AbError *error) {
    PreludeFixture *fixture = context;
    uintptr_t value = 0;
    bool present = false;
    if (!ab_string_map_get(fixture->hats, name, &value, &present, error))
        return false;
    *output = present ? (AbBirdCollisionHat *)value : NULL;
    return true;
}

static bool damage_score(void *context, float damage, float *output, AbError *error) {
    (void)context;
    return ab_damage_score(damage, output) || ab_fail(error, "Invalid fixture damage");
}

static bool score_entry(void *context, AbSpan name, AbBirdCollisionScoreEntry **output,
                        AbError *error) {
    (void)error;
    PreludeFixture *fixture = context;
    CHECK(name.size == 4 && !memcmp(name.data, "bird", 4));
    *output = &fixture->score;
    return true;
}

static bool floating(void *context, const AbBirdCollisionFloatingScore *request,
                     AbError *error) {
    PreludeFixture *fixture = context;
    return ab_floating_scores_add(fixture->floating, request, error);
}

static bool real_prelude_storage(void) {
    PreludeFixture fixture = {0};
    CHECK(ab_floating_scores_open((AbFloatingScoreLimits){2, 32, 4096},
                                  &fixture.floating, NULL) &&
          ab_string_map_open(0, 2, 4096, &fixture.hats, NULL));
    AbBirdCollisionObject first = {.object = {.name = span("bird")},
                                   .position = {2, 3}};
    AbBirdCollisionObject second = {.object = {.name = span("block")}};
    float entries[2];
    AbBirdCollisionPreludeState state = {
        .floating_score_scaling = 1, .collision_scores = entries, .capacity = 2};
    AbBirdCollisionPreludeHost host = {.context = &fixture,
                                       .hat = hat_lookup,
                                       .score_entry = score_entry,
                                       .score = damage_score,
                                       .floating = floating};
    bool related = true;
    CHECK(ab_bird_collision_prelude(span("bird"), &first, &second, 3.5f, &state, &host,
                                    &related, NULL) &&
          !related && state.count == 1 && entries[0] == 30 && fixture.score.present &&
          fixture.score.score == 30 && ab_floating_scores_count(fixture.floating) == 1);
    AbBirdCollisionFloatingScore *score = ab_floating_scores_at(fixture.floating, 0);
    CHECK(score && score->text.size == 2 && !memcmp(score->text.data, "30", 2) &&
          score->position.x == 2 && score->position.y == 3 && score->time == 0 &&
          score->lifetime == 0.6f && score->max_scale == 0.26f && score->xs == 0 &&
          ab_floating_scores_update(fixture.floating, 0.125f, NULL) &&
          score->time == 0.125f && score->xs == 0.13f);
    ab_string_map_close(fixture.hats);
    ab_floating_scores_close(fixture.floating);
    return true;
}

int main(void) {
    return storage_and_timing() && real_prelude_storage() && removal_storage() ? 0 : 1;
}
