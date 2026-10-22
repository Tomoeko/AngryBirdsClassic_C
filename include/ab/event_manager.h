#ifndef AB_EVENT_MANAGER_H
#define AB_EVENT_MANAGER_H

#include "ab/lua_data.h"

typedef struct AbEventManager AbEventManager;
typedef struct AbEventListener AbEventListener;

struct AbEventListener {
    AbSpan name;
    void *context;
    bool (*event_triggered)(void *context, AbDataTable *event, AbError *error);
};

typedef enum {
    AB_EVENT_DIAGNOSTIC_NOTIFY,
    AB_EVENT_DIAGNOSTIC_INVALID_LISTENER,
    AB_EVENT_DIAGNOSTIC_DUPLICATE,
    AB_EVENT_DIAGNOSTIC_EMPTY_REMOVAL,
} AbEventDiagnostic;

typedef struct {
    void *context;
    bool (*diagnostic)(void *context, AbEventDiagnostic kind, AbDataValue id,
                       AbEventListener *listener, AbDataTable *event, AbError *error);
} AbEventManagerHost;

typedef struct {
    size_t event_types, listeners_per_type, queued_events, delayed_events;
    size_t notification_depth, storage_bytes;
} AbEventManagerLimits;

typedef struct {
    size_t event_types, queued_events, buffered_events, delayed_events;
    bool buffering;
} AbEventManagerView;

/* Bounded owner for EventManager's dense listener/queue/delay domain. IDs are
 * plain string/number/Boolean/table keys; NaN keys reject. Event and listener
 * objects, names and callback contexts stay borrowed until removed/drained.
 * IDs and table storage stay valid through all callbacks; ID string bytes stay
 * immutable even when an event's id field is replaced. No payload copies,
 * VM, metatables, sparse sequences or internal queue-table aliases are exposed.
 * Own fixed arrays once, sharing storage_bytes; no frame allocation.
 *
 * Optional diagnostics represent the original print/printError boundaries;
 * omitting them disables diagnostic observation, not event delivery. Hosts may
 * add/remove listeners, queue events and issue nested notifications. Serialize
 * calls; never close or reenter tick from a callback. Depth (1..64)/capacity limits
 * reject explicitly. A late failure retains preceding effects. Output starts
 * NULL; close releases all storage without firing game events. */
bool ab_event_manager_open(AbEventManagerLimits limits, AbEventManagerHost host,
                           AbEventManager **output, AbError *error);
void ab_event_manager_close(AbEventManager *manager);
size_t ab_event_manager_bytes(const AbEventManager *manager);
AbEventManagerView ab_event_manager_view(const AbEventManager *manager);

bool ab_event_manager_add(AbEventManager *manager, AbDataValue id,
                          AbEventListener *listener, AbError *error);
/* Subscription requires a string/number ID because the original constructs
 * its assertion message before callback validation, even for a valid listener. */
/* An absent/empty listener list is an error. A nonempty list without this
 * identity is preserved successfully. Remove matches last to first. */
bool ab_event_manager_remove(AbEventManager *manager, AbDataValue id,
                             AbEventListener *listener, AbError *error);
/* Validate non-nil event.id, cache its listener list, then visit the live dense
 * sequence forwards. Appended listeners are visited; removals retain original
 * ipairs index progression. Event mutations remain visible to later listeners.
 * Diagnostic ID concatenation requires a string/number when a listener exists.
 * Read each listener's callback after its diagnostic boundary. */
bool ab_event_manager_notify(AbEventManager *manager, AbDataTable *event,
                             AbError *error);
bool ab_event_manager_queue(AbEventManager *manager, AbDataTable *event,
                            AbError *error);
/* Write event.delay before appending its borrowed identity. Duplicate event
 * identities are retained and share the same mutable delay field. */
bool ab_event_manager_delay(AbEventManager *manager, AbDataTable *event, float delay,
                            AbError *error);
/* Cache the current queued length and buffer new queue requests during that
 * dispatch. Replace the consumed queue with buffered events, then decrement all
 * current delayed entries in order. Notify expired identities and remove their
 * first current occurrence afterward. Delayed callbacks queue for the next
 * tick; newly delayed entries in those callbacks wait for that next tick too.
 * Finite seconds/delays/results only; no implicit clock or delta cap. */
bool ab_event_manager_tick(AbEventManager *manager, float seconds, AbError *error);

#endif
