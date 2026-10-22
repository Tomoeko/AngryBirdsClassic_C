#ifndef AB_UI_LOCALIZATION_H
#define AB_UI_LOCALIZATION_H

#include "ab/base.h"

typedef struct {
    AbSpan group;
    bool present;
} AbUiLocalizationCache;

typedef struct {
    const AbSpan *groups;
    size_t group_count;
    void *context;
    bool (*get_string)(void *context, AbSpan group, AbSpan identifier, AbSpan *output,
                       AbError *error);
} AbUiLocalizationSource;

/* Original getLocalizationString group selection with platform/profiles already
 * prepared. Try an optional cached group first. A result different from the
 * identifier, including empty text, succeeds. Otherwise traverse the dense
 * profile group order, skipping entries equal to the cached group. Successful
 * profile lookup stores that group only when a cache object was supplied.
 * Always fall back to TEXTS_BASIC when unresolved, without changing cache;
 * an empty identifier goes directly to this fallback. No provider lookup is
 * coalesced, including repeated BASIC fallback or duplicate profile groups.
 * Stable borrowed text/groups/cache and nonmutating provider callbacks are
 * required, count <=2^24. No allocation or resource loading. Provider results
 * borrow their owner. Failure preserves output; earlier callback effects remain.
 * Original requireFile dependency preparation and malformed Lua types are
 * outside this resolved-provider boundary. */
bool ab_ui_localization_string(const AbUiLocalizationSource *source, AbSpan identifier,
                               AbUiLocalizationCache *cache, AbSpan *output,
                               AbError *error);

#endif
