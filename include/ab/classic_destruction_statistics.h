#ifndef AB_CLASSIC_DESTRUCTION_STATISTICS_H
#define AB_CLASSIC_DESTRUCTION_STATISTICS_H

#include "ab/classic_level.h"
#include "ab/event_manager.h"
#include "ab/settings_statistics.h"

typedef struct AbClassicDestructionStatistics AbClassicDestructionStatistics;

typedef struct {
    AbClassicLevel *level;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbDataValue *eagle_bait_launched, *device_model, *gamer_service;
    float *total_blocks;
    void *context;
    bool (*per_project)(void *context, size_t object, const AbDataTable **output,
                        AbError *error);
    size_t notifications, event_fields, storage_bytes;
} AbClassicDestructionStatisticsConfig;

/* updateDestroyedBlocksValues prototype 134861. Follow the actual eagle/platform/
 * gamerService gates, material event before its local increment, sequential
 * device sums and final aggregate notification. Unknown non-nil materials need
 * the supplied AchievementProcessor continuation. Nil material preserves the
 * previous totalBlocks value. Extra result fields may replace aggregate keys.
 * Borrow canonical state/settings/events/live globals and returned values.
 * Event identities stay owned and stable until close; no frame allocation or
 * slot reuse. Drain observers before close. No save or online response is added.
 * Source-backed C composition; full original-method execution parity is separate.
 */
bool ab_classic_destruction_statistics_open(AbClassicDestructionStatisticsConfig config,
                                            AbClassicDestructionStatistics **output,
                                            AbError *error);
void ab_classic_destruction_statistics_close(AbClassicDestructionStatistics *owner);
size_t
ab_classic_destruction_statistics_bytes(const AbClassicDestructionStatistics *owner);
bool ab_classic_destruction_statistics_call(void *context, size_t object,
                                            AbError *error);

#endif
