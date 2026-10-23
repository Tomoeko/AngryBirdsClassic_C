#ifndef AB_SETTINGS_GOLDEN_EGGS_H
#define AB_SETTINGS_GOLDEN_EGGS_H

#include "ab/settings_statistics.h"

/* SettingsWrapper getGoldenEgg20563/isUnlocked21001/unlock21122. Use the
 * actual highscore owner, with nil-only openGoldenEggLevels/row creation and
 * the original two-field constructor. Queries preserve raw field values.
 * Borrow key bytes until storage closes. No persistence or reward presentation;
 * callers retain the source save/event order around mutations. */
bool ab_settings_golden_egg(AbSettingsStatistics *highscores, AbSpan name,
                            AbDataValue *output, AbError *error);
bool ab_settings_golden_egg_unlocked(AbSettingsStatistics *highscores, AbSpan name,
                                     AbDataValue *output, AbError *error);
/* isGoldenEggPlayed20882 returns the same owned row's raw opened field. */
bool ab_settings_golden_egg_played(AbSettingsStatistics *highscores, AbSpan name,
                                   AbDataValue *output, AbError *error);
bool ab_settings_golden_egg_unlock(AbSettingsStatistics *highscores, AbSpan name,
                                   AbError *error);

#endif
