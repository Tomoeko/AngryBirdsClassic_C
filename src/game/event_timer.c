#include "ab/event_timer.h"

#include <math.h>

static bool valid(const AbEventTimer *timer, AbError *error) {
    return timer && timer->initialized && !timer->failed &&
                   timer->count <= timer->capacity &&
                   (!timer->capacity || timer->entries)
               ? true
               : ab_fail(error, "Invalid or failed event timer");
}

bool ab_event_timer_init(AbEventTimer *timer, AbEventTimerEntry *entries,
                         size_t capacity, float start, AbError *error) {
    if (!timer || (capacity && !entries) || capacity > UINT32_C(16777216) ||
        !isfinite(start))
        return ab_fail(error, "Invalid event-timer storage/start");
    *timer = (AbEventTimer){
        .entries = entries, .capacity = capacity, .time = start, .initialized = true};
    return true;
}

bool ab_event_timer_once(AbEventTimer *timer, AbDataTable *event,
                         AbEventListener *listener, float delay, AbError *error) {
    if (!valid(timer, error))
        return false;
    float deadline = timer->time + delay;
    if (timer->count == timer->capacity || !isfinite(delay) || !isfinite(deadline))
        return ab_fail(error, "Event timer exceeds capacity or finite time domain");
    size_t index = timer->count++;
    timer->entries[index] = (AbEventTimerEntry){deadline, event, listener};
    while (index) {
        size_t parent = (index - 1) / 2;
        if (!(deadline < timer->entries[parent].deadline))
            break;
        AbEventTimerEntry previous = timer->entries[parent];
        timer->entries[parent] = timer->entries[index];
        timer->entries[index] = previous;
        index = parent;
    }
    return true;
}

static AbEventTimerEntry remove_first(AbEventTimer *timer) {
    AbEventTimerEntry first = timer->entries[0];
    AbEventTimerEntry last = timer->entries[--timer->count];
    if (!timer->count)
        return first;
    timer->entries[0] = last;
    size_t index = 0;
    while (index < timer->count / 2) {
        size_t left = index * 2 + 1, right = left + 1;
        bool left_fits =
            left < timer->count && !(last.deadline < timer->entries[left].deadline);
        bool right_fits =
            right < timer->count && !(last.deadline < timer->entries[right].deadline);
        if (!left_fits && !right_fits)
            break;
        size_t next = left_fits && (!right_fits || timer->entries[left].deadline <
                                                       timer->entries[right].deadline)
                          ? left
                          : right;
        timer->entries[index] = timer->entries[next];
        timer->entries[next] = last;
        index = next;
    }
    return first;
}

bool ab_event_timer_update(AbEventTimer *timer, float seconds, AbError *error) {
    if (!valid(timer, error) || timer->updating)
        return ab_fail(error, "Reentrant or failed event-timer update");
    float time = timer->time + seconds;
    if (!isfinite(seconds) || !isfinite(time))
        return ab_fail(error, "Event timer update exceeds finite time domain");
    timer->time = time;
    timer->updating = true;
    bool okay = true;
    while (timer->count && timer->entries[0].deadline < timer->time) {
        AbEventTimerEntry entry = remove_first(timer);
        if (!entry.listener || !entry.listener->event_triggered) {
            okay = ab_fail(error, "Missing reached event-timer listener callback");
            break;
        }
        if (!entry.listener->event_triggered(entry.listener->context, entry.event,
                                             error)) {
            okay = false;
            break;
        }
    }
    timer->updating = false;
    timer->failed = !okay;
    return okay;
}
