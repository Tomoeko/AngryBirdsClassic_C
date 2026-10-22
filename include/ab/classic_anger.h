#ifndef AB_CLASSIC_ANGER_H
#define AB_CLASSIC_ANGER_H

#include "ab/event_manager.h"
#include "ab/session_timer.h"

typedef struct AbClassicAnger AbClassicAnger;

typedef struct {
    AbDataValue initialized, maximum, boost_cost, level, charges, queued, used;
    AbSessionTimerState level_timer, cooldown_timer;
} AbClassicAngerState;

typedef struct {
    AbDataValue maximum, level, charges, queued;
    AbSessionTimerSave timer, cooldown;
} AbClassicAngerSettings;

typedef struct {
    AbClassicAngerState *state;
    /* NULL current value models absent/false settings. Retain published records
     * until close. Imported settings remain borrowed until their next load. */
    AbClassicAngerSettings **settings;
    AbSessionClock clock;
    AbEventManager *events;
    AbDataValue load_level_late, level_increased;
    void *context;
    bool (*is_in_level)(void *context, AbDataValue *output, AbError *error);
    bool (*save_settings)(void *context, AbError *error);
    bool (*load_refill_popup)(void *context, AbError *error);
    size_t records, iteration_limit, storage_bytes;
} AbClassicAngerConfig;

/* Ordinary AngerLevel init/update/refill/save/event owner. Real EventManager and
 * SessioningTimer execute directly; settings serialization, LevelFlow query and
 * popup assembly require their actual providers when reached. No substitute
 * service responses. Open binds existing typed state without running source
 * init; update lazily initializes when initialized is nil/false.
 *
 * Own bounded distinct event/save records once. Borrow IDs/state/settings and
 * callback contexts. Drain retained events before close, and close before the
 * borrowed EventManager. No frame allocation. Numeric state and typed calendar
 * save tables only; arbitrary method overrides/table identity are excluded.
 * Catch-up beyond iteration_limit rejects explicitly. Late failure stops owner
 * and retains previous changes. Update recursion rejects; event callbacks may
 * enter the other operations. Hero boost/upgrade/UI methods remain separate. */
bool ab_classic_anger_open(AbClassicAngerConfig config, AbClassicAnger **output,
                           AbError *error);
void ab_classic_anger_close(AbClassicAnger *anger);
size_t ab_classic_anger_bytes(const AbClassicAnger *anger);
bool ab_classic_anger_init(AbClassicAnger *anger, AbError *error);
bool ab_classic_anger_update(AbClassicAnger *anger, AbError *error);
bool ab_classic_anger_add(AbClassicAnger *anger, float amount, AbError *error);
bool ab_classic_anger_activate_cooldown(AbClassicAnger *anger, AbError *error);
bool ab_classic_anger_save(AbClassicAnger *anger, AbError *error);
bool ab_classic_anger_load(AbClassicAnger *anger, AbError *error);
bool ab_classic_anger_event(AbClassicAnger *anger, AbDataTable *event, AbError *error);

#endif
