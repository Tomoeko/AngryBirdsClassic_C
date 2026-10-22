#ifndef AB_URL_REQUESTS_H
#define AB_URL_REQUESTS_H

#include "ab/lua_data.h"
#include "ab/string_map.h"

typedef struct AbUrlRequests AbUrlRequests;

typedef struct {
    void *context;
    bool (*call)(void *context, AbSpan url, AbSpan response, AbDataValue extra,
                 AbError *error);
} AbUrlResponseCall;

typedef struct {
    void *context;
    bool (*call)(void *context, AbDataValue extra, AbError *error);
} AbUrlTimeoutCall;

typedef struct {
    AbDataValue timer, extra;
    AbUrlResponseCall *response;
    AbUrlTimeoutCall *timeout;
} AbUrlRequest;

typedef enum {
    AB_URL_START_LOG,
    AB_URL_RESPONSE_URL_LOG,
    AB_URL_RESPONSE_BODY_LOG,
} AbUrlLog;

typedef struct {
    void *context;
    bool (*start)(void *context, AbSpan url, AbError *error);
    bool (*log)(void *context, AbUrlLog kind, AbSpan text, AbError *error);
} AbUrlRequestHost;

/* Actual URLRequests.lua module. Own the source-initialized empty string map,
 * bounded retained request records and reusable private expired-key map. Borrow
 * pinned URL bytes, callback records and extra values. No network substitute:
 * start requires the real native/platform provider when reached. Omitted log
 * disables diagnostic observation only. No allocation after open.
 *
 * String keys, finite used timer arithmetic, fixed map identity and native
 * string-only hash iteration. Callbacks may replace records, add requests and
 * change fields. Each original request identity remains alive until close.
 * Start/update/response recursion rejects; cross-operation calls remain supported.
 * Late failure stops the owner and retains earlier fields/map/callback effects.
 */
bool ab_url_requests_open(size_t records, size_t map_nodes, size_t storage_bytes,
                          AbUrlRequestHost host, AbUrlRequests **output,
                          AbError *error);
void ab_url_requests_close(AbUrlRequests *requests);
size_t ab_url_requests_bytes(const AbUrlRequests *requests);
const AbStringMap *ab_url_requests_map(const AbUrlRequests *requests);
bool ab_url_requests_lookup(AbUrlRequests *requests, AbSpan url, AbUrlRequest **output,
                            AbError *error);
/* Source startURLFetch226: nil/false timer defaults10; raw zero stays0.
 * Store fresh request before log/start. Replacing a URL does not cancel an
 * earlier native fetch. Callable absence is preserved until actually used. */
bool ab_url_requests_start(AbUrlRequests *requests, AbSpan url,
                           AbUrlResponseCall *response, AbDataValue duration,
                           AbUrlTimeoutCall *timeout, AbDataValue extra,
                           AbError *error);
/* Source update529 decrements ALL timers, then copies strictly-negative entries
 * into a fresh private map, then calls optional timeouts and removes each URL.
 * A replacement of the same URL during its timeout is removed afterward too. */
bool ab_url_requests_update(AbUrlRequests *requests, float seconds, AbError *error);
/* Source nativecallback_urlResponse806 logs before lookup. A present request
 * calls its response with the original url/body and live extra, then removes
 * the URL. Replacements made by that callback are removed too. Missing URLs
 * produce diagnostics only. Response transport/data parsing are caller work. */
bool ab_url_requests_response(AbUrlRequests *requests, AbSpan url, AbSpan response,
                              AbError *error);

#endif
