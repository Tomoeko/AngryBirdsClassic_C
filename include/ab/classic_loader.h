#ifndef AB_CLASSIC_LOADER_H
#define AB_CLASSIC_LOADER_H

#include "ab/event_manager.h"

typedef struct AbClassicLoader AbClassicLoader;

typedef struct {
    void *context;
    bool (*call)(void *context, AbError *error);
} AbClassicLoaderCall;

typedef struct {
    AbClassicLoaderCall **items;
    size_t capacity;
} AbClassicLoaderCalls;

typedef struct {
    AbDataValue loading, frame, loading_frame, minimum_display_time, completion_event;
    AbClassicLoaderCalls *items;
    AbClassicLoaderCall *completion_call;
} AbClassicLoaderState;

typedef struct {
    AbDataValue id, hidden, loading_page_alpha, minimum_display_time, completion_event;
    AbClassicLoaderCalls *items;
    AbClassicLoaderCall *completion_call;
} AbClassicLoaderRequest;

typedef enum {
    AB_CLASSIC_LOADER_NO_MINIMUM,
    AB_CLASSIC_LOADER_MINIMUM_PASSED,
} AbClassicLoaderLog;

typedef struct {
    AbClassicLoaderState *state;
    const AbDataValue *script_time, *do_loading, *show_loading, *hide_loading;
    AbDataValue *skip_input;
    AbEventManager *events;
    size_t notifications, storage_bytes;
    void *context;
    bool (*log)(void *context, AbClassicLoaderLog kind, AbError *error);
} AbClassicLoaderConfig;

/* Actual Loader eventTriggered426/finishLoading936/update1220. Borrow canonical
 * state, live globals/event IDs, real events, requests and callable records.
 * Own distinct retained show/hide event payloads once; later calls allocate
 * nothing. Outer SubSystem initialization/registration and actual loading-page/
 * resource factory callbacks remain caller work. Source init265 assigns false
 * loading and numeric frame0; bind those real initialized fields explicitly.
 *
 * Numeric finite frame/deadline arithmetic; dense callable prefix ending at
 * the first NULL. Original function fields use live callback records; other
 * truthy noncallable fields are unsupported. Preserve captured old item storage
 * across replacement. Callbacks may change fields and issue another operation;
 * same-operation recursion and update during another operation reject.
 * Earlier state/events survive failure, and the owner then stops.
 */
bool ab_classic_loader_open(AbClassicLoaderConfig config, AbClassicLoader **output,
                            AbError *error);
void ab_classic_loader_close(AbClassicLoader *loader);
size_t ab_classic_loader_bytes(const AbClassicLoader *loader);
/* Show notification precedes copying request fields; listeners may change the
 * borrowed request before those copies. A truthy min time adds current script
 * time, including numeric zero. Source completion-event identity is retained. */
bool ab_classic_loader_event(AbClassicLoader *loader, AbClassicLoaderRequest *request,
                             AbError *error);
bool ab_classic_loader_finish(AbClassicLoader *loader, AbError *error);
/* Original seconds/time parameters are unused. Read the shared script-time
 * global only at a reached minimum-display check. Always increment frame,
 * including while loading is false; do not use an integer tick counter. */
bool ab_classic_loader_update(AbClassicLoader *loader, AbError *error);

#endif
