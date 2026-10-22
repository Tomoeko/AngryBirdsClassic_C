#include "ab/url_requests.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbUrlRequests {
    AbUrlRequestHost host;
    AbStringMap *map, *expired;
    AbUrlRequest *records;
    size_t record_count, record_capacity, bytes;
    bool starting, updating, responding, failed;
};

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached URL timer");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached URL timer");
}

bool ab_url_requests_open(size_t records, size_t map_nodes, size_t storage_bytes,
                          AbUrlRequestHost host, AbUrlRequests **output,
                          AbError *error) {
    if (!output)
        return ab_fail(error, "Missing URL requests output");
    *output = NULL;
    if (!records || records > (SIZE_MAX - sizeof(AbUrlRequests)) / sizeof(AbUrlRequest))
        return ab_fail(error, "Invalid URL request record bound");
    size_t bytes = sizeof(AbUrlRequests) + records * sizeof(AbUrlRequest);
    if (bytes > storage_bytes)
        return ab_fail(error, "URL request records exceed storage bound");
    AbUrlRequests *requests = calloc(1, bytes);
    if (!requests)
        return ab_fail(error, "Cannot allocate URL request records");
    requests->host = host;
    requests->records = (AbUrlRequest *)(requests + 1);
    requests->record_capacity = records;
    requests->bytes = bytes;
    if (!ab_string_map_open(0, map_nodes, storage_bytes - bytes, &requests->map, error))
        goto release_requests;
    requests->bytes += ab_string_map_bytes(requests->map);
    if (!ab_string_map_open(0, map_nodes, storage_bytes - requests->bytes,
                            &requests->expired, error))
        goto release_requests;
    requests->bytes += ab_string_map_bytes(requests->expired);
    *output = requests;
    return true;

release_requests:
    ab_url_requests_close(requests);
    return false;
}

void ab_url_requests_close(AbUrlRequests *requests) {
    if (!requests)
        return;
    ab_string_map_close(requests->map);
    ab_string_map_close(requests->expired);
    free(requests);
}

size_t ab_url_requests_bytes(const AbUrlRequests *requests) {
    return requests ? requests->bytes : 0;
}

const AbStringMap *ab_url_requests_map(const AbUrlRequests *requests) {
    return requests ? requests->map : NULL;
}

bool ab_url_requests_lookup(AbUrlRequests *requests, AbSpan url, AbUrlRequest **output,
                            AbError *error) {
    if (!requests || !output)
        return ab_fail(error, "Invalid URL request lookup");
    uintptr_t value;
    bool present;
    if (!ab_string_map_get(requests->map, url, &value, &present, error))
        return false;
    *output = present ? (AbUrlRequest *)value : NULL;
    return true;
}

static bool log_value(AbUrlRequests *requests, AbUrlLog kind, AbSpan text,
                      AbError *error) {
    return !requests->host.log ||
           requests->host.log(requests->host.context, kind, text, error);
}

bool ab_url_requests_start(AbUrlRequests *requests, AbSpan url,
                           AbUrlResponseCall *response, AbDataValue duration,
                           AbUrlTimeoutCall *timeout, AbDataValue extra,
                           AbError *error) {
    if (!requests || requests->failed || requests->starting ||
        requests->record_count == requests->record_capacity)
        return ab_fail(error, "Invalid, stopped or exhausted URL request start");
    requests->starting = true;
    AbUrlRequest *request = requests->records + requests->record_count++;
    AbDataValue default_time = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x41200000)};
    *request = (AbUrlRequest){truthy(duration) ? duration : default_time, extra,
                              response, timeout};
    bool okay =
        ab_string_map_set(requests->map, url, (uintptr_t)request, true, error) &&
        log_value(requests, AB_URL_START_LOG, url, error);
    if (okay)
        okay = requests->host.start
                   ? requests->host.start(requests->host.context, url, error)
                   : ab_fail(error, "Missing reached native URL fetch provider");
    requests->starting = false;
    requests->failed = requests->failed || !okay;
    return okay;
}

static bool collect_expired(AbUrlRequests *requests, float seconds, AbError *error) {
    if (!ab_string_map_reset(requests->expired, 0, error))
        return false;
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(requests->map, &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        AbUrlRequest *request = (AbUrlRequest *)entry.value;
        float timer;
        if (!numeric(request->timer, &timer, error) || !isfinite(seconds))
            return ab_fail(error, "Invalid reached URL timer arithmetic");
        timer -= seconds;
        if (!isfinite(timer))
            return ab_fail(error, "URL timer arithmetic overflow");
        request->timer = (AbDataValue){.kind = AB_DATA_NUMBER};
        memcpy(&request->timer.bits, &timer, sizeof(timer));
        if (timer < 0 &&
            !ab_string_map_set(requests->expired, entry.key, entry.value, true, error))
            return false;
    }
}

static bool notify_expired(AbUrlRequests *requests, AbError *error) {
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(requests->expired, &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        AbUrlRequest *request = (AbUrlRequest *)entry.value;
        if (request->timeout) {
            AbUrlTimeoutCall *timeout = request->timeout;
            if (!timeout->call)
                return ab_fail(error, "Reached noncallable URL timeout");
            if (!timeout->call(timeout->context, request->extra, error))
                return false;
        }
        if (!ab_string_map_set(requests->map, entry.key, 0, false, error))
            return false;
    }
}

bool ab_url_requests_update(AbUrlRequests *requests, float seconds, AbError *error) {
    if (!requests || requests->failed || requests->updating)
        return ab_fail(error, "Invalid or stopped URL timer update");
    requests->updating = true;
    bool okay =
        collect_expired(requests, seconds, error) && notify_expired(requests, error);
    requests->updating = false;
    requests->failed = requests->failed || !okay;
    return okay;
}

bool ab_url_requests_response(AbUrlRequests *requests, AbSpan url, AbSpan response,
                              AbError *error) {
    if (!requests || requests->failed || requests->responding)
        return ab_fail(error, "Invalid or stopped URL response");
    requests->responding = true;
    AbUrlRequest *request = NULL;
    bool okay = log_value(requests, AB_URL_RESPONSE_URL_LOG, url, error) &&
                log_value(requests, AB_URL_RESPONSE_BODY_LOG, response, error) &&
                ab_url_requests_lookup(requests, url, &request, error);
    if (okay && request) {
        AbUrlResponseCall *callback = request->response;
        okay = callback && callback->call
                   ? callback->call(callback->context, url, response, request->extra,
                                    error)
                   : ab_fail(error, "Missing reached URL response callback");
        if (okay)
            okay = ab_string_map_set(requests->map, url, 0, false, error);
    }
    requests->responding = false;
    requests->failed = requests->failed || !okay;
    return okay;
}
