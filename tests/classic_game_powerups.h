#ifndef AB_TEST_CLASSIC_GAME_POWERUPS_H
#define AB_TEST_CLASSIC_GAME_POWERUPS_H

#include "ab/classic_game_powerups.h"

typedef struct {
    AbClassicGamePowerups *owner;
    AbClassicShots *shots;
    AbClassicPowerupUpdate **active;
    AbDataValue *skip;
    float *timer;
    size_t first_calls, second_calls, passive_calls;
    bool fail;
} LifecyclePowerups;

static bool lifecycle_powerup_first(void *context, float seconds, AbError *error) {
    LifecyclePowerups *test = context;
    (void)error;
    CHECK(seconds == 0.125f && test->skip->kind == AB_DATA_BOOLEAN &&
          test->skip->bits && !ab_classic_shots_state(test->shots)->dragged &&
          !ab_classic_game_powerups_update(test->owner, seconds, NULL));
    ++test->first_calls;
    /* Removing current dense-array index1 shifts index2 into index1. Native
     * pairs next resumes after the old numeric key, skipping that shifted row. */
    test->active[0] = test->active[1];
    test->active[1] = NULL;
    AbDataValue transitioning;
    CHECK(ab_classic_game_powerups_transitioning(test->owner, &transitioning, error) &&
          transitioning.kind == AB_DATA_BOOLEAN && transitioning.bits);
    return true;
}

static bool lifecycle_powerup_second(void *context, float seconds, AbError *error) {
    LifecyclePowerups *test = context;
    CHECK(seconds == 0.125f && !ab_classic_shots_state(test->shots)->dragged &&
          test->first_calls == 1 && test->passive_calls >= 1);
    ++test->second_calls;
    return !test->fail || ab_fail(error, "Intentional active powerup update failure");
}

static bool lifecycle_powerup_passive(void *context, float seconds, AbError *error) {
    LifecyclePowerups *test = context;
    (void)error;
    CHECK(seconds == 0.125f && test->first_calls == 1 &&
          test->second_calls == test->passive_calls);
    ++test->passive_calls;
    *test->timer = 2;
    *test->skip = (AbDataValue){0};
    return true;
}

/* Controlled active/passive records exercise source scheduling over the real
 * sling cancellation. Actual powerup factories/effects/bootstrap remain open. */
static bool lifecycle_powerups(ShotTest *shot, AbClassicShots *shots,
                               AbClassicSling *sling, AbError *error) {
    AbClassicGamePowerups owner = {0};
    AbDataValue enabled = {0}, cheats = {0}, skip = {0};
    float speed = NAN, timer = 0;
    AbDataEntry keys[] = {{{.kind = AB_DATA_STRING, .string = span("1")}, {0}},
                          {{.kind = AB_DATA_STRING, .string = span("2")}, {0}}};
    AbDataTable released_keys = {keys, 2, 2, NULL};
    AbClassicPowerupUpdate *active[2] = {0}, *passive[1] = {0};
    AbClassicPowerupUpdates active_list = {active, 2}, passive_list = {passive, 1};
    AbClassicGamePowerupConfig config = {.sling = sling,
                                         .active = &active_list,
                                         .passive = &passive_list,
                                         .enabled = &enabled,
                                         .cheats_enabled = &cheats,
                                         .released_keys = &released_keys,
                                         .physics_speed_factor = &speed,
                                         .skip_input = &skip,
                                         .game_timer = &timer};
    AbDataValue transitioning;
    CHECK(ab_classic_game_powerups_initialize(config, &owner, error) &&
          ab_classic_game_powerups_transitioning(&owner, &transitioning, error) &&
          transitioning.kind == AB_DATA_BOOLEAN && !transitioning.bits &&
          ab_classic_game_powerups_update(&owner, 0.125f, error) && timer == 0.125f &&
          skip.kind == AB_DATA_NIL);
    enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_game_powerups_update(&owner, 0.125f, error) && timer == 0.25f &&
          skip.kind == AB_DATA_NIL);
    LifecyclePowerups test = {&owner, shots, active, &skip, &timer, 0, 0, 0, false};
    AbClassicPowerupUpdate first = {.context = &test,
                                    .update = lifecycle_powerup_first};
    AbClassicPowerupUpdate second = {.context = &test,
                                     .update = lifecycle_powerup_second};
    AbClassicPowerupUpdate last = {.context = &test,
                                   .update = lifecycle_powerup_passive};
    active[0] = &first;
    active[1] = &second;
    passive[0] = &last;
    CHECK(ab_classic_game_powerups_transitioning(&owner, &transitioning, error) &&
          transitioning.kind == AB_DATA_BOOLEAN && transitioning.bits);
    speed = 1;
    size_t object;
    CHECK(ab_classic_sling_next_bird(sling, 1, &object, error) && object != SIZE_MAX);
    ab_classic_shots_state(shots)->dragged = ab_classic_shots_bird(shots, object);
    CHECK(ab_classic_game_powerups_update(&owner, 0.125f, error) &&
          !ab_classic_shots_state(shots)->dragged &&
          ab_classic_shots_bird(shots, object)->position.x == shot->rest.x &&
          ab_classic_shots_bird(shots, object)->position.y == shot->rest.y &&
          test.first_calls == 1 && !test.second_calls && test.passive_calls == 1 &&
          timer == 2.125f && skip.kind == AB_DATA_NIL);
    speed = 0;
    CHECK(ab_classic_game_powerups_update(&owner, 0.125f, error) &&
          test.second_calls == 1 && test.passive_calls == 2 && timer == 2.125f &&
          skip.kind == AB_DATA_NIL);
    speed = 1;
    test.fail = true;
    AbError late = {0};
    CHECK(!ab_classic_game_powerups_update(&owner, 0.125f, &late) &&
          strstr(late.message, "Intentional active powerup update failure") &&
          test.second_calls == 2 && test.passive_calls == 2 && timer == 2.125f &&
          skip.kind == AB_DATA_BOOLEAN && skip.bits && owner.failed &&
          !ab_classic_game_powerups_transitioning(&owner, &transitioning, NULL) &&
          !ab_classic_game_powerups_update(&owner, 0.125f, NULL) &&
          test.second_calls == 2);
    active[0] = active[1] = passive[0] = NULL;
    cheats = (AbDataValue){.kind = AB_DATA_NUMBER};
    keys[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbClassicGamePowerups cheat = {0};
    CHECK(ab_classic_sling_view(sling).current_bird_index == 0 &&
          ab_classic_game_powerups_initialize(config, &cheat, error) &&
          ab_classic_game_powerups_update(&cheat, 0, error) && timer == 2.125f);
    keys[1].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbError missing = {0};
    CHECK(!ab_classic_game_powerups_update(&cheat, 0.125f, &missing) &&
          strstr(missing.message, "Missing actual powerup activation service") &&
          timer == 2.125f);
    return true;
}

#endif
