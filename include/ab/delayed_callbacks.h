#ifndef AB_DELAYED_CALLBACKS_H
#define AB_DELAYED_CALLBACKS_H

#include "ab/classic_game_updates.h"
#include "ab/lua_data.h"

typedef struct AbDelayedCallbacks AbDelayedCallbacks;

typedef struct {
    void *context;
    bool (*call)(void *context, AbDataValue parameters, AbError *error);
} AbDelayedFunction;

/* delayedCallback.lua159/224/423 from the pinned APK. Own the private numeric
 * registry, timeout records and shallow-copy snapshot; borrow stable callback
 * records/parameters and game_updates until close. Limits bound concurrent live
 * callbacks, not total calls. The consecutive binary32 ID domain ends before
 * 2^24; exhaustion is explicit. No frame allocation or implicit clock scaling.
 * Open retains nil state; initialize at the actual level-load reset boundary. */
bool ab_delayed_callbacks_open(AbClassicGameUpdates *game_updates, size_t capacity,
                               size_t bytes, AbDelayedCallbacks **output,
                               AbError *error);
void ab_delayed_callbacks_close(AbDelayedCallbacks *callbacks);
size_t ab_delayed_callbacks_bytes(const AbDelayedCallbacks *callbacks);
bool ab_delayed_callbacks_initialize(AbDelayedCallbacks *callbacks, AbError *error);
bool ab_delayed_callbacks_add(AbDelayedCallbacks *callbacks, float timeout,
                              const AbDelayedFunction *function, AbDataValue parameters,
                              AbError *error);
/* Original unscaled seconds. Each call traverses a fresh shallow copy in native
 * numeric-table order. New callbacks wait until a subsequent call. Resetting the
 * current table during a callback preserves the captured records, and expiry
 * deletes that key from the then-current table. The registered update function
 * remains installed when empty. Reentry/failure stops further use. */
bool ab_delayed_callbacks_update(AbDelayedCallbacks *callbacks, float seconds,
                                 AbError *error);

#endif
