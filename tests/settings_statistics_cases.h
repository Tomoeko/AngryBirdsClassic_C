#ifndef AB_TEST_SETTINGS_STATISTICS_CASES_H
#define AB_TEST_SETTINGS_STATISTICS_CASES_H

#include <stdint.h>

enum { STATISTICS_CASES = 64, STATISTICS_ACTIONS = 16 };
enum {
    STATISTICS_GET_LOCAL,
    STATISTICS_SET_LOCAL,
    STATISTICS_GET_SUM,
    STATISTICS_INCREMENT_BIRDS,
    STATISTICS_INCREMENT_BACKWARDS,
};

typedef struct {
    uint32_t operation, counter, kind, bits;
} StatisticsAction;

static const char *const statistics_counters[] = {"birdsShooted", "backwardsBirdCount"};

static const StatisticsAction statistics_actions[STATISTICS_ACTIONS] = {
    {STATISTICS_GET_LOCAL, 0, 0, 0},
    {STATISTICS_GET_SUM, 0, 0, 0},
    {STATISTICS_INCREMENT_BIRDS, 0, 0, 0},
    {STATISTICS_INCREMENT_BACKWARDS, 1, 0, 0},
    {STATISTICS_GET_SUM, 1, 0, 0},
    {STATISTICS_SET_LOCAL, 0, 1, 0},
    {STATISTICS_GET_LOCAL, 0, 0, 0},
    {STATISTICS_INCREMENT_BIRDS, 0, 0, 0},
    {STATISTICS_SET_LOCAL, 1, 3, 0x4b800000},
    {STATISTICS_INCREMENT_BACKWARDS, 1, 0, 0},
    {STATISTICS_GET_LOCAL, 1, 0, 0},
    {STATISTICS_GET_SUM, 1, 0, 0},
    {STATISTICS_SET_LOCAL, 0, 0, 0},
    {STATISTICS_INCREMENT_BIRDS, 0, 0, 0},
    {STATISTICS_SET_LOCAL, 1, 3, 0x80000000},
    {STATISTICS_GET_SUM, 1, 0, 0},
};

/* Fixture values only; both implementations use independently authored code. */
static const uint32_t statistics_seed_values[] = {0x00000000, 0x80000000, 0x3fc00000,
                                                  0x4b800000};

#endif
