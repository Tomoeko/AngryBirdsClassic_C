#include "ab/classic_game_globals.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Shared game globals failed at %d\n", __LINE__);           \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool clock_order(void) {
    AbClassicGameGlobals globals = {0};
    CHECK(ab_classic_game_globals_advance_time(&globals, NAN, NULL) &&
          globals.time.kind == AB_DATA_NUMBER && globals.time.bits == number(0).bits);
    globals.time = number(-0.0f);
    CHECK(ab_classic_game_globals_advance_time(&globals, 0.25f, NULL) &&
          globals.time.bits == number(0.25f).bits);
    globals.time = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_classic_game_globals_advance_time(&globals, INFINITY, NULL) &&
          globals.time.bits == number(0).bits);
    globals.time = number(3);
    globals.score = number(80);
    globals.last_score_time = globals.time;
    globals.old_score = globals.score;
    CHECK(ab_classic_game_globals_advance_time(&globals, 1, NULL) &&
          globals.time.bits == number(4).bits &&
          globals.last_score_time.bits == number(3).bits &&
          ab_classic_game_globals_level_time(&globals, 0.125f, NULL) &&
          globals.time_spent.bits == number(0.125f).bits);
    globals.time_spent = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_classic_game_globals_level_time(&globals, 0.25f, NULL) &&
          globals.time_spent.bits == number(0.25f).bits);
    globals.time = number(16777216);
    CHECK(ab_classic_game_globals_advance_time(&globals, 1, NULL) &&
          globals.time.bits == number(16777216).bits &&
          !ab_classic_game_globals_advance_time(&globals, NAN, NULL) &&
          globals.time.bits == number(16777216).bits);
    return true;
}

static bool timeout(void) {
    AbClassicGameGlobals globals = {.time = number(4),
                                    .score = number(80),
                                    .old_score = number(80),
                                    .last_score_time = number(3)};
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, 0.125f,
                                          globals.time, NULL) &&
          globals.last_score_timer.bits == number(4).bits &&
          !globals.level_timeout.bits);
    /* The accumulated timer, not a discontinuous current-time argument, drives
     * expiration until a score change resets both timers to that argument. */
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, 13.5f,
                                          number(100), NULL) &&
          globals.last_score_timer.bits == number(17.5f).bits &&
          !globals.level_timeout.bits &&
          ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, 0.5f, number(100),
                                          NULL) &&
          globals.level_timeout.bits);
    globals.score = number(81);
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, 1, number(100),
                                          NULL) &&
          !globals.level_timeout.bits && globals.old_score.bits == number(81).bits &&
          globals.last_score_time.bits == number(100).bits &&
          globals.last_score_timer.bits == number(100).bits);
    globals.last_score_timer = number(-0.0f);
    globals.last_score_time = number(-1);
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, 0.5f, number(100),
                                          NULL) &&
          globals.last_score_timer.bits == number(0.5f).bits);
    globals.last_score_timer = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, NAN, number(2),
                                          NULL) &&
          globals.last_score_timer.bits == number(2).bits);
    globals.level_timeout.bits = 1;
    CHECK(
        ab_classic_game_globals_timeout(&globals, number(0), NAN, number(NAN), NULL) &&
        !globals.level_timeout.bits && globals.last_score_timer.bits == number(2).bits);
    globals.last_score_time = number(-0.0f);
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, NAN, number(NAN),
                                          NULL) &&
          globals.last_score_timer.bits == number(2).bits);
    globals.last_score_time = (AbDataValue){0};
    globals.last_score_timer = (AbDataValue){0};
    globals.old_score = number(0);
    globals.score = number(1);
    CHECK(ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, NAN, number(5),
                                          NULL) &&
          globals.last_score_time.bits == number(5).bits &&
          globals.old_score.bits == number(1).bits);
    globals.last_score_time = (AbDataValue){0};
    CHECK(!ab_classic_game_globals_timeout(&globals, (AbDataValue){0}, 1, number(6),
                                           NULL) &&
          globals.last_score_timer.bits == number(6).bits &&
          !globals.level_timeout.bits);
    return true;
}

static bool shot_view(void) {
    AbClassicGameGlobals globals = {.time = number(4),
                                    .score = number(80),
                                    .last_score_time = number(3),
                                    .old_score = number(79),
                                    .extra_tutorial_timer = number(0.125f),
                                    .specialty_available = number(0)};
    AbShotState shot = {.time = -10, .score = -10, .birds_shot = 2};
    CHECK(ab_classic_game_globals_refresh_shot(&globals, &shot, NULL) &&
          shot.time == 4 && shot.score == 80 && shot.last_score_time == 3 &&
          shot.old_score == 79 && shot.birds_shot == 2 &&
          shot.tutorial_timer == 0.125f && shot.specialty_available);
    shot.last_score_time = shot.time;
    shot.old_score = shot.score;
    CHECK(ab_classic_game_globals_publish_shot(&globals, &shot, NULL) &&
          globals.last_score_time.bits == number(4).bits &&
          globals.old_score.bits == number(80).bits);
    CHECK(ab_classic_game_globals_publish_score(&globals, 5640, NULL) &&
          ab_classic_game_globals_refresh_shot(&globals, &shot, NULL) &&
          shot.score == 5640 && shot.old_score == 80);
    globals.time = number(5);
    globals.old_score = (AbDataValue){0};
    CHECK(!ab_classic_game_globals_refresh_shot(&globals, &shot, NULL) &&
          shot.time == 4 && shot.score == 5640 && shot.old_score == 80);
    return true;
}

int main(void) {
    if (!clock_order() || !timeout() || !shot_view())
        return 1;
    puts("Shared gameplay clocks, timeout and shot score publication checks passed");
    return 0;
}
