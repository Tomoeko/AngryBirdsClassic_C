#ifndef AB_EVENT_TIMER_H
#define AB_EVENT_TIMER_H

#include "ab/event_manager.h"

typedef struct {
    float deadline;
    AbDataTable *event;
    AbEventListener *listener;
} AbEventTimerEntry;

typedef struct {
    AbEventTimerEntry *entries;
    size_t count, capacity;
    float time;
    bool initialized, updating, failed;
} AbEventTimer;

/* timer.lua Timer/BinaryHeap over caller-owned fixed storage. Events/listeners
 * stay borrowed through delivery; NULL event represents source nil. Preserve
 * binary32 time+delay, strict deadline<time, strict insertion comparisons and
 * right-child tie selection while removing. Removal precedes the live callback.
 * Callbacks may schedule more events; a newly due event runs in the same update.
 * Reentrant update is outside this contract. Late failures stop the owner and
 * retain time/dequeue/callback effects. No allocation or VM. */
bool ab_event_timer_init(AbEventTimer *timer, AbEventTimerEntry *entries,
                         size_t capacity, float start, AbError *error);
bool ab_event_timer_once(AbEventTimer *timer, AbDataTable *event,
                         AbEventListener *listener, float delay, AbError *error);
bool ab_event_timer_update(AbEventTimer *timer, float seconds, AbError *error);

#endif
