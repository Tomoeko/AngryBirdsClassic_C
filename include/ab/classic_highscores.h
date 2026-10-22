#ifndef AB_CLASSIC_HIGHSCORES_H
#define AB_CLASSIC_HIGHSCORES_H

#include "ab/settings_statistics.h"
#include "ab/event_manager.h"

typedef struct AbClassicHighscores AbClassicHighscores;

typedef enum {
    AB_HIGHSCORE_MAP,
    AB_HIGHSCORE_CHAPTER,
    AB_HIGHSCORE_RESET_LEADERBOARDS,
} AbClassicHighscoreService;

typedef struct {
    AbSettingsStatistics *storage;
    AbEventManager *events;
    const AbDataValue *separate_powerups, *started_from_map, *started_sublevel;
    const AbDataValue *chapter_service, *native_gamer_service;
    AbDataValue new_highscore_event;
    size_t notifications, storage_bytes;
    void *context;
    bool (*service)(void *context, AbClassicHighscoreService operation,
                    AbDataValue selection, AbSpan level, float score, float birds,
                    AbDataValue powerups, AbError *error);
} AbClassicHighscoresConfig;

/* Concrete highscore_utils.lua147/506/856 over the actual plain highscore graph.
 * Preserve nil-only migration, raw truthiness, score/bird writes before
 * notifications, separate assisted records and low-score comparison. This
 * owner supplies no completion flag, default result, save policy or UI.
 * Map/chapter/native gamer calls require real reached services.
 * All keys, flags, event IDs, storage and callbacks remain borrowed until close.
 * Open has no source effects. Own distinct bounded notification identities;
 * drain consumers before close. Mutations survive a late failure; stop/discard
 * the borrowing session. String level IDs and finite numeric comparisons only.
 * No frame allocation, file I/O, arbitrary graph replacement or script VM. */
bool ab_classic_highscores_open(AbClassicHighscoresConfig config,
                                AbClassicHighscores **output, AbError *error);
void ab_classic_highscores_close(AbClassicHighscores *highscores);
bool ab_classic_highscores_initialize(AbClassicHighscores *highscores, AbSpan level,
                                      AbError *error);
bool ab_classic_highscores_current(AbClassicHighscores *highscores, AbSpan level,
                                   AbDataValue powerups, AbDataValue *output,
                                   AbError *error);
bool ab_classic_highscores_improve(AbClassicHighscores *highscores, AbSpan level,
                                   float score, float birds, AbDataValue powerups,
                                   AbError *error);

#endif
