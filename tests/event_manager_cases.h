#ifndef AB_TEST_EVENT_MANAGER_CASES_H
#define AB_TEST_EVENT_MANAGER_CASES_H

#include <stdint.h>

enum {
    EVENT_MANAGER_CASES = 256,
    EVENT_MANAGER_ACTIONS = 24,
    EVENT_MANAGER_SNAPSHOT_WORDS = 23,
    EVENT_MANAGER_TRACE_WORDS = 25,
    EVENT_MANAGER_TRACE_CAPACITY = 128,
};

typedef enum {
    EVENT_FIXTURE_ADD,
    EVENT_FIXTURE_REMOVE,
    EVENT_FIXTURE_NOTIFY,
    EVENT_FIXTURE_QUEUE,
    EVENT_FIXTURE_DELAY,
    EVENT_FIXTURE_TICK,
    EVENT_FIXTURE_CHANGE_ID,
} EventFixtureOperation;

typedef struct {
    EventFixtureOperation operation;
    uint32_t id, object, number;
} EventFixtureAction;

/* Explicit service environment, independent of EventManager's algorithms.
 * The two delayed entries intentionally alias the same mutable event. */
static const EventFixtureAction event_fixture_actions[EVENT_MANAGER_ACTIONS] = {
    {EVENT_FIXTURE_ADD, 0, 0, 0},
    {EVENT_FIXTURE_ADD, 0, 1, 0},
    {EVENT_FIXTURE_ADD, 1, 1, 0},
    {EVENT_FIXTURE_QUEUE, 0, 0, 0},
    {EVENT_FIXTURE_DELAY, 0, 1, UINT32_C(0x3e000000)},
    {EVENT_FIXTURE_DELAY, 0, 1, UINT32_C(0x3e800000)},
    {EVENT_FIXTURE_QUEUE, 0, 2, 0},
    {EVENT_FIXTURE_TICK, 0, 0, UINT32_C(0x3e000000)},
    {EVENT_FIXTURE_NOTIFY, 0, 0, 0},
    {EVENT_FIXTURE_TICK, 0, 0, UINT32_C(0x3e000000)},
    {EVENT_FIXTURE_TICK, 0, 0, UINT32_C(0xbe000000)},
    {EVENT_FIXTURE_REMOVE, 0, 0, 0},
    {EVENT_FIXTURE_NOTIFY, 0, 0, 0},
    {EVENT_FIXTURE_TICK, 0, 0, UINT32_C(0x3f800000)},
    {EVENT_FIXTURE_REMOVE, 1, 1, 0},
    {EVENT_FIXTURE_CHANGE_ID, 1, 0, 0},
    {EVENT_FIXTURE_QUEUE, 0, 0, 0},
    {EVENT_FIXTURE_TICK, 0, 0, 0},
    {EVENT_FIXTURE_DELAY, 0, 2, UINT32_C(0x80000000)},
    {EVENT_FIXTURE_TICK, 0, 0, 0},
    {EVENT_FIXTURE_NOTIFY, 0, 3, 0},
    {EVENT_FIXTURE_ADD, 0, 1, 0},
    {EVENT_FIXTURE_REMOVE, 0, 2, 0},
    {EVENT_FIXTURE_TICK, 0, 0, UINT32_C(0x3f000000)},
};

static const EventFixtureAction event_fixture_effects[8] = {
    {EVENT_FIXTURE_QUEUE, 0, 3, 0},
    {EVENT_FIXTURE_DELAY, 0, 2, UINT32_C(0x3d800000)},
    {EVENT_FIXTURE_REMOVE, 0, 1, 0},
    {EVENT_FIXTURE_ADD, 0, 2, 0},
    {EVENT_FIXTURE_ADD, 0, 1, 0}, /* Disable its callback instead of registering. */
    {EVENT_FIXTURE_CHANGE_ID, 1, 0, 0},
    {EVENT_FIXTURE_QUEUE, 0, 0, 0},
    {EVENT_FIXTURE_NOTIFY, 0, 2, 0},
};

#endif
