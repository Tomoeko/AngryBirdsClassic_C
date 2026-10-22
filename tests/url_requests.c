#include "ab/url_requests.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "URL requests failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct UrlTest UrlTest;
typedef struct {
    UrlTest *test;
    unsigned id;
} UrlContext;

struct UrlTest {
    AbUrlRequests *owner;
    AbUrlResponseCall responses[4];
    AbUrlTimeoutCall timeouts[4];
    UrlContext contexts[4];
    AbUrlRequest *old[4];
    uint32_t rows[32][8];
    size_t count;
    unsigned case_id;
    bool fail;
};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static void value_words(AbDataValue value, uint32_t *output) {
    static const uint32_t kinds[] = {0, 1, 3, 4, 5};
    output[0] = kinds[value.kind];
    output[1] =
        value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN ? value.bits : 0;
}

static AbSpan url(unsigned id) {
    static const char *names[] = {"u0", "u1", "u2", "u3"};
    return span(names[id]);
}

static unsigned url_id(AbSpan value) {
    return value.size == 2 && value.data && value.data[0] == 'u' &&
                   value.data[1] >= '0' && value.data[1] <= '3'
               ? (unsigned)(value.data[1] - '0')
               : 4;
}

static bool record(UrlTest *test, unsigned kind, unsigned id, AbDataValue extra) {
    CHECK(test->count < 32 && id < 4);
    uint32_t *row = test->rows[test->count++];
    memset(row, 0, sizeof(test->rows[0]));
    row[0] = kind;
    row[1] = id;
    value_words(extra, row + 2);
    for (unsigned index = 0; index < 4; ++index) {
        AbUrlRequest *request = NULL;
        CHECK(ab_url_requests_lookup(test->owner, url(index), &request, NULL));
        row[4 + index] = request ? request->timer.bits : UINT32_MAX;
    }
    return true;
}

static bool start(UrlTest *test, unsigned index, unsigned duration, float extra,
                  bool omit_timeout) {
    static const float times[] = {0, 0, 0.25f, 0.5f, 0, -1, 2, 3};
    AbDataValue timer = duration == 1   ? (AbDataValue){0}
                        : duration == 4 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                                        : number(times[duration]);
    return ab_url_requests_start(test->owner, url(index), test->responses + index,
                                 timer, omit_timeout ? NULL : test->timeouts + index,
                                 number(extra), NULL);
}

static bool response(void *context, AbSpan address, AbSpan body, AbDataValue extra,
                     AbError *error) {
    UrlContext *binding = context;
    UrlTest *test = binding->test;
    CHECK(url_id(address) == binding->id && body.size == 4 &&
          !memcmp(body.data, "body", 4) &&
          !ab_url_requests_response(test->owner, address, body, NULL) &&
          record(test, 5, binding->id, extra));
    if ((test->case_id & 4) && binding->id == 2)
        CHECK(start(test, 2, 7, 88, false));
    return !test->fail || ab_fail(error, "Intentional URL callback failure");
}

static bool timeout(void *context, AbDataValue extra, AbError *error) {
    UrlContext *binding = context;
    UrlTest *test = binding->test;
    CHECK(!ab_url_requests_update(test->owner, 0, NULL) &&
          record(test, 6, binding->id, extra));
    if ((test->case_id & 4) && !binding->id) {
        AbUrlRequest *other = NULL;
        CHECK(ab_url_requests_lookup(test->owner, url(1), &other, error) && other);
        other->extra = number(77);
        CHECK(start(test, 0, 6, 99, false));
    }
    return !test->fail || ab_fail(error, "Intentional URL timeout failure");
}

static bool fetch(void *context, AbSpan address, AbError *error) {
    (void)error;
    return record(context, 2, url_id(address), (AbDataValue){0});
}

static bool log_value(void *context, AbUrlLog kind, AbSpan value, AbError *error) {
    (void)error;
    unsigned id = kind == AB_URL_RESPONSE_BODY_LOG ? 0 : url_id(value);
    return record(context,
                  kind == AB_URL_START_LOG          ? 1u
                  : kind == AB_URL_RESPONSE_URL_LOG ? 3u
                                                    : 4u,
                  id, (AbDataValue){0});
}

static bool prepare(UrlTest *test, unsigned case_id) {
    *test = (UrlTest){.case_id = case_id};
    for (unsigned index = 0; index < 4; ++index) {
        test->contexts[index] = (UrlContext){test, index};
        test->responses[index] = (AbUrlResponseCall){test->contexts + index, response};
        test->timeouts[index] = (AbUrlTimeoutCall){test->contexts + index, timeout};
    }
    CHECK(ab_url_requests_open(32, 16, 65536,
                               (AbUrlRequestHost){test, fetch, log_value}, &test->owner,
                               NULL) &&
          ab_url_requests_bytes(test->owner) <= 65536);
    return true;
}

static bool compare(FILE *reference, const uint32_t *expected, size_t count) {
    if (!reference)
        return true;
    for (size_t index = 0; index < count; ++index) {
        uint32_t actual;
        CHECK(fscanf(reference, "%" SCNx32, &actual) == 1);
        if (actual != expected[index]) {
            fprintf(stderr, "URL word %zu: %08" PRIx32 " != %08" PRIx32 "\n", index,
                    actual, expected[index]);
            return false;
        }
    }
    return true;
}

static bool snapshot(UrlTest *test, FILE *reference, unsigned step) {
    uint32_t header[] = {test->case_id, step, (uint32_t)test->count};
    CHECK(compare(reference, header, 3));
    for (unsigned index = 0; index < 4; ++index) {
        uint32_t values[5] = {0};
        AbUrlRequest *request = NULL;
        CHECK(ab_url_requests_lookup(test->owner, url(index), &request, NULL));
        if (request) {
            values[0] = 1;
            value_words(request->timer, values + 1);
            value_words(request->extra, values + 3);
        }
        CHECK(compare(reference, values, 5));
    }
    for (unsigned index = 0; index < 4; ++index) {
        uint32_t values[2] = {0};
        if (test->old[index])
            value_words(test->old[index]->timer, values);
        CHECK(compare(reference, values, 2));
    }
    for (size_t index = 0; index < test->count; ++index)
        CHECK(compare(reference, test->rows[index], 8));
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_URL_REQUESTS_V1 8 9\n"));
    }
    for (unsigned case_id = 0; case_id < 8; ++case_id) {
        UrlTest test;
        CHECK(prepare(&test, case_id));
        for (unsigned step = 0; step < 9; ++step) {
            test.count = 0;
            if (step == 1) {
                CHECK(start(&test, 0, case_id & 1 ? 1u : 0u, 10, false) &&
                      start(&test, 1, 2, 11, (case_id & 2) != 0) &&
                      start(&test, 2, 3, 12, false) && start(&test, 3, 4, 13, false));
                for (unsigned index = 0; index < 4; ++index)
                    CHECK(ab_url_requests_lookup(test.owner, url(index),
                                                 test.old + index, NULL));
            } else if (step == 2 || step == 4 || step == 5 || step == 8)
                CHECK(ab_url_requests_update(test.owner,
                                             step == 2   ? 0.25f
                                             : step == 5 ? 0.125f
                                                         : 0,
                                             NULL));
            else if (step == 3 || step == 6)
                CHECK(ab_url_requests_response(test.owner, url(2), span("body"), NULL));
            else if (step == 7)
                CHECK(start(&test, 1, 5, 55, false));
            CHECK(snapshot(&test, reference, step));
        }
        ab_url_requests_close(test.owner);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB URL requests reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbUrlRequests *owner = NULL;
    CHECK(!ab_url_requests_open(0, 16, 65536, (AbUrlRequestHost){0}, &owner, NULL) &&
          !owner &&
          !ab_url_requests_open(SIZE_MAX, 16, SIZE_MAX, (AbUrlRequestHost){0}, &owner,
                                NULL) &&
          !owner &&
          ab_url_requests_open(4, 4, 65536, (AbUrlRequestHost){0}, &owner, NULL) &&
          ab_url_requests_update(owner, NAN, NULL));
    AbError error = {0};
    CHECK(!ab_url_requests_start(owner, url(0), NULL, (AbDataValue){0}, NULL, number(1),
                                 &error) &&
          strstr(error.message, "native URL fetch provider"));
    AbUrlRequest *retained = NULL;
    CHECK(ab_url_requests_lookup(owner, url(0), &retained, NULL) && retained &&
          retained->timer.bits == number(10).bits &&
          !ab_url_requests_update(owner, 0, NULL));
    ab_url_requests_close(owner);
    UrlTest test;
    CHECK(prepare(&test, 0) && start(&test, 0, 0, 10, false));
    test.fail = true;
    CHECK(!ab_url_requests_update(test.owner, 0.25f, &error) &&
          strstr(error.message, "Intentional URL timeout failure") &&
          ab_url_requests_lookup(test.owner, url(0), &retained, NULL) && retained &&
          retained->timer.bits == number(-0.25f).bits &&
          !ab_url_requests_response(test.owner, url(0), span("body"), NULL));
    ab_url_requests_close(test.owner);
    CHECK(prepare(&test, 0) && start(&test, 0, 1, 10, false) &&
          ab_url_requests_lookup(test.owner, url(0), &retained, NULL) && retained);
    retained->response = NULL;
    CHECK(!ab_url_requests_response(test.owner, url(0), span("body"), &error) &&
          strstr(error.message, "response callback") &&
          ab_url_requests_lookup(test.owner, url(0), &retained, NULL) && retained);
    ab_url_requests_close(test.owner);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference
             ? "URL requests match 72 original state/callback checkpoints"
             : "URL request provider, callback and partial failure checks passed");
    return 0;
}
