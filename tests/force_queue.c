#include "ab/force_queue.h"
#include "ab/object_forces.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Force queue failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static float from_bits(uint32_t bits) {
    float number;
    memcpy(&number, &bits, sizeof(number));
    return number;
}

static uint32_t to_bits(float number) {
    uint32_t bits;
    memcpy(&bits, &number, sizeof(bits));
    return bits;
}

static const char *const names[] = {"first", "second", "third"};

static uint32_t object_id(AbSpan object) {
    for (size_t index = 0; index < 3; ++index) {
        if (object.size == strlen(names[index]) &&
            memcmp(object.data, names[index], object.size) == 0) {
            return (uint32_t)index + 1;
        }
    }
    return 0;
}

typedef struct {
    uint32_t mask, mutation, pass;
    uint32_t events[32][6];
    size_t count;
    size_t fail_at;
} QueueHost;

static bool exists(void *context, AbSpan name, bool *present, AbError *error) {
    (void)error;
    QueueHost *host = context;
    uint32_t id = object_id(name);
    CHECK(id);
    *present = (host->mask & (1u << (id - 1))) != 0;
    return true;
}

static bool apply(void *context, const AbQueuedForce *force, AbError *error) {
    (void)error;
    QueueHost *host = context;
    CHECK(host->count < 32);
    uint32_t *event = host->events[host->count++];
    event[0] = host->pass;
    event[1] = object_id(force->object);
    event[2] = to_bits(force->force.x);
    event[3] = to_bits(force->force.y);
    event[4] = to_bits(force->point.x);
    event[5] = to_bits(force->point.y);
    host->mask ^= host->mutation;
    return !host->fail_at || host->count != host->fail_at;
}

static bool edge_checks(void) {
    AbQueuedForce entries[2] = {0};
    AbForceQueue queue = {entries, 0, 2};
    AbQueuedForce force = {span("first"), {1, -2}, {4, 7}};
    CHECK(!ab_force_queue_append(NULL, force, NULL));
    CHECK(!ab_force_queue_clear(NULL, NULL));
    CHECK(ab_force_queue_apply(&queue, (AbForceQueueHost){0}, NULL));
    CHECK(!ab_force_queue_apply(NULL, (AbForceQueueHost){0}, NULL));
    CHECK(ab_force_queue_append(&queue, force, NULL));
    AbQueuedForce saved = entries[0];
    force.force.x = NAN;
    CHECK(!ab_force_queue_append(&queue, force, NULL) && queue.count == 1 &&
          memcmp(&saved, entries, sizeof(saved)) == 0);
    force.force.x = 1;
    CHECK(ab_force_queue_append(&queue, force, NULL));
    CHECK(!ab_force_queue_append(&queue, force, NULL) && queue.count == 2);
    CHECK(!ab_force_queue_apply(&queue, (AbForceQueueHost){0}, NULL));
    QueueHost host = {.mask = 1, .fail_at = 2};
    AbForceQueueHost calls = {&host, exists, apply};
    CHECK(!ab_force_queue_apply(&queue, calls, NULL) && host.count == 2 &&
          queue.count == 2);
    CHECK(ab_force_queue_clear(&queue, NULL) && queue.count == 0 &&
          memcmp(&saved, entries, sizeof(saved)) == 0);
    queue.count = 3;
    CHECK(!ab_force_queue_clear(&queue, NULL) && queue.count == 3);
    queue.count = 0;
    queue.entries = NULL;
    CHECK(!ab_force_queue_append(&queue, force, NULL));
    AbBodyMotion body = {.mass = {.type = AB_BODY_DYNAMIC, .center = {2, 3}},
                         .sleep_time = 7};
    CHECK(ab_object_force_at(&body, (AbVec2){1, -2}, (AbVec2){4, 7}, NULL));
    CHECK(body.awake && body.sleep_time == 0 && body.force.x == 1 &&
          body.force.y == -2 && body.torque == -8);
    AbBodyMotion before = body;
    CHECK(!ab_object_force_at(&body, (AbVec2){NAN, 0}, (AbVec2){0}, NULL) &&
          memcmp(&before, &body, sizeof(body)) == 0);
    body.mass.type = AB_BODY_STATIC;
    before = body;
    CHECK(ab_object_force_at(&body, (AbVec2){NAN, NAN}, (AbVec2){NAN, NAN}, NULL) &&
          memcmp(&before, &body, sizeof(body)) == 0);
    CHECK(!ab_object_force_at(NULL, (AbVec2){0}, (AbVec2){0}, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_FORCE_QUEUE_V1 4096\n") == 0);
    size_t total = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[4], requests[8][5], expected[2];
        CHECK(read_words(file, setup, 4) && setup[0] <= 8 && setup[1] <= 3 &&
              setup[2] <= 7 && setup[3] <= 7);
        for (size_t index = 0; index < 8; ++index) {
            CHECK(read_words(file, requests[index], 5));
        }
        CHECK(read_words(file, expected, 2) && expected[1] <= 32);
        AbQueuedForce storage[8], captured[8];
        AbForceQueue queue = {storage, 0, 8};
        QueueHost host = {.mutation = setup[3]};
        AbForceQueueHost calls = {&host, exists, apply};
        for (size_t index = 0; index < 8; ++index) {
            const uint32_t *input = requests[index];
            CHECK(input[0] >= 1 && input[0] <= 3);
            captured[index] =
                (AbQueuedForce){span(names[input[0] - 1]),
                                {from_bits(input[1]), from_bits(input[2])},
                                {from_bits(input[3]), from_bits(input[4])}};
            if (index < setup[0]) {
                CHECK(ab_force_queue_append(&queue, captured[index], NULL));
            }
        }
        for (host.pass = 0; host.pass < setup[1]; ++host.pass) {
            host.mask = (setup[2] ^ (host.pass * 3)) & 7;
            CHECK(ab_force_queue_apply(&queue, calls, NULL) && queue.count == setup[0]);
        }
        CHECK(ab_force_queue_clear(&queue, NULL));
        host.pass = 3;
        CHECK(ab_force_queue_apply(&queue, calls, NULL));
        CHECK(ab_force_queue_append(&queue, captured[0], NULL));
        host.pass = 4;
        host.mask = 7;
        CHECK(ab_force_queue_apply(&queue, calls, NULL));
        CHECK(ab_force_queue_clear(&queue, NULL));
        host.pass = 5;
        CHECK(ab_force_queue_apply(&queue, calls, NULL));
        CHECK(host.mask == expected[0] && host.count == expected[1] &&
              queue.count == 0);
        for (size_t index = 0; index < host.count; ++index) {
            uint32_t event[6];
            CHECK(read_words(file, event, 6));
            if (memcmp(host.events[index], event, sizeof(event)) != 0) {
                fprintf(stderr, "Native queue case %zu event %zu differs\n", item,
                        index);
                return false;
            }
        }
        total += host.count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB force queue reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native queued-force cases and %zu ordered applications match\n",
           total);
    return true;
}

static bool point_native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_FORCE_POINT_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12], expected[5];
        CHECK(read_words(file, input, 12) && read_words(file, expected, 5));
        AbBodyMotion body = {
            .mass = {.type = (AbBodyType)input[0],
                     .center = {from_bits(input[3]), from_bits(input[4])}},
            .force = {from_bits(input[5]), from_bits(input[6])},
            .torque = from_bits(input[7]),
            .awake = input[1] != 0,
            .sleep_time = from_bits(input[2])};
        CHECK(ab_object_force_at(
            &body, (AbVec2){from_bits(input[8]), from_bits(input[9])},
            (AbVec2){from_bits(input[10]), from_bits(input[11])}, NULL));
        uint32_t actual[] = {36 | (body.awake ? 2u : 0u), to_bits(body.force.x),
                             to_bits(body.force.y), to_bits(body.torque),
                             to_bits(body.sleep_time)};
        if (memcmp(actual, expected, sizeof(actual)) != 0) {
            fprintf(stderr, "Native point-force case %zu differs\n", item);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB force point reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native point-force accumulation and wake results match");
    return true;
}

int main(int argc, char **argv) {
    bool point = argc == 3 && strcmp(argv[1], "--point") == 0;
    if ((argc > 2 && !point) || !edge_checks()) {
        return 1;
    }
    if (argc >= 2) {
        FILE *file = fopen(argv[point ? 2 : 1], "rb");
        bool okay = file && (point ? point_native_checks(file) : native_checks(file));
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts("Queued force bounds, partial effects, clearing and point application pass");
    return 0;
}
