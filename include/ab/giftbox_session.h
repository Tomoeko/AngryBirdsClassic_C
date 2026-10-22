#ifndef AB_GIFTBOX_SESSION_H
#define AB_GIFTBOX_SESSION_H

#include "ab/session_timer.h"
#include "ab/settings_statistics.h"

typedef struct {
    AbSettingsStatistics *settings;
    const AbDataValue *remote_config;
    AbSessionClock clock;
    bool initialized, busy, failed;
} AbGiftBoxSession;

/* Local GiftBoxManager.updateGiftBoxSession3979 and SettingsWrapper's attempt
 * and reset operations. Borrow the actual settings owner and live raw remote
 * configuration binding; explicit nil means absent configuration. No service
 * responses, gift placement, claims, rewards, notification or save scheduling
 * are supplied here. Initialization has no settings or clock effects.
 *
 * Missing/false saved start uses 2000-01-01 without storing the fallback. Session
 * thresholds default to day=0/hour=7/minutes=0/seconds=0; only non-nil configured
 * fields override them. Compare absolute calendar parts in source short-circuit
 * order. Only reached numeric fields must be finite numbers. Other coercions,
 * metatables and extra date fields are outside this projection. A reset calls
 * the clock again, deep-copies its six fields in native table iteration order,
 * sets seconds=0, then publishes current number=1, fresh spawn/claim maps and
 * attempt=0. Total number and current spawn level stay untouched.
 *
 * Access is serialized, ordinary calls allocate nothing. Errors stop the owner;
 * bounded settings exhaustion retains completed writes, including a partially
 * built date table. Original allocation-failure behavior is not modeled. Removed
 * tables retain their identity until the settings owner closes. */
bool ab_giftbox_session_initialize(AbGiftBoxSession *session,
                                   AbSettingsStatistics *settings,
                                   const AbDataValue *remote_config,
                                   AbSessionClock clock, AbError *error);
bool ab_giftbox_session_increase_attempt(AbGiftBoxSession *session, AbError *error);
bool ab_giftbox_session_update(AbGiftBoxSession *session, AbError *error);

#endif
