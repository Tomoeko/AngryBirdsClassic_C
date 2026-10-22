#ifndef AB_SESSION_TIMER_H
#define AB_SESSION_TIMER_H

#include "ab/calendar_time.h"
#include "ab/lua_data.h"

typedef struct {
    float hours, minutes, seconds;
} AbSessionDuration;

typedef struct {
    AbDataValue active, force_completed, duration_seconds;
    const AbCalendarTime *start;
    const AbSessionDuration *duration;
} AbSessionTimerState;

typedef struct {
    void *context;
    bool (*now)(void *context, AbCalendarTime *output, AbError *error);
} AbSessionClock;

/* Typed deep copies of the source save table. Both flags false represent the
 * fresh empty table returned by an inactive timer. No table identities escape. */
typedef struct {
    bool has_start, has_duration;
    AbCalendarTime start;
    AbSessionDuration duration;
} AbSessionTimerSave;

typedef struct {
    AbSessionTimerState *state;
    AbSessionClock clock;
    AbCalendarTime owned_start;
    AbSessionDuration owned_duration;
    bool initialized, busy, failed;
} AbSessionTimer;

/* SessioningTimer.lua numeric date/duration projection. Borrow state and explicit
 * start/duration arguments; their fields remain live until replaced. Clock and
 * loaded dates use owned snapshots. Save/load copy numeric fields, with no
 * arbitrary Lua table graph, overridden methods or exposed snapshot identity.
 * No allocation. A reached clock requires its real provider. Callbacks may edit
 * state; operations reread reached fields afterward. Recursive calls reject;
 * late failure stops the owner and retains preceding field/callback effects.
 * Initialize, and source init on repeat, set active=false ONLY. Neither start,
 * init, load nor deactivation clears force_completed. */
bool ab_session_timer_initialize(AbSessionTimer *timer, AbSessionTimerState *state,
                                 AbSessionClock clock, AbError *error);
bool ab_session_timer_init(AbSessionTimer *timer, AbError *error);
bool ab_session_timer_force_complete(AbSessionTimer *timer, AbError *error);
bool ab_session_timer_set_active(AbSessionTimer *timer, AbDataValue active,
                                 AbError *error);
/* A NULL start models absent/false: call clock before publishing start/duration.
 * Publish both pointers before duration conversion. Cache (hours*3600 +
 * minutes*60) + seconds, then set active=true. No calendar normalization here. */
bool ab_session_timer_start(AbSessionTimer *timer, const AbSessionDuration *duration,
                            const AbCalendarTime *start, AbError *error);
/* Inactive remaining/complete return nil. A truthy force flag is returned raw
 * by complete, including numeric zero, without consulting the clock. */
bool ab_session_timer_remaining(AbSessionTimer *timer, AbDataValue *output,
                                AbError *error);
bool ab_session_timer_complete(AbSessionTimer *timer, AbDataValue *output,
                               AbError *error);
/* Inactive progress/formatted return numeric zero. Progress is not clamped;
 * zero duration preserves IEEE division results. Format uses source formatTime
 * hours/minutes/seconds and last-two-character padding, including negatives.
 * String output borrows the caller buffer, which must outlive its consumer. */
bool ab_session_timer_progress(AbSessionTimer *timer, float *output, AbError *error);
bool ab_session_timer_formatted(AbSessionTimer *timer, char *buffer, size_t capacity,
                                AbDataValue *output, AbError *error);
/* Copy start, then add each duration component without normalizing date fields.
 * Invalid start/duration reject; source diagnostic formatting is not projected. */
bool ab_session_timer_end(AbSessionTimer *timer, AbCalendarTime *output,
                          AbError *error);
bool ab_session_timer_save(AbSessionTimer *timer, AbSessionTimerSave *output,
                           AbError *error);
/* NULL/missing-field input sets active=false directly, preserving old fields.
 * Valid input deep-copies start/duration, then follows startTimer ordering. */
bool ab_session_timer_load(AbSessionTimer *timer, const AbSessionTimerSave *input,
                           AbError *error);
/* Explicit adapter to the native platform calendar boundary. */
bool ab_session_clock_platform(void *context, AbCalendarTime *output, AbError *error);

#endif
