#include "ab/physics_contact_state.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

_Static_assert(sizeof(AbManifold) == 16 * sizeof(uint32_t),
               "Native contact comparison requires 16 words");

typedef struct {
    AbManifold evaluation;
    uint32_t events[2][18];
    size_t event_count;
    uint32_t mode;
    bool overlap;
    bool invalid;
} ContactProbe;

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t to_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void evaluate(void *context, AbManifold *output) {
    ContactProbe *probe = context;
    *output = probe->evaluation;
}

static bool overlap(void *context) {
    return ((ContactProbe *)context)->overlap;
}

static void record_event(ContactProbe *probe, uint32_t kind, AbContactState *contact,
                         const AbManifold *previous) {
    if (probe->event_count >= 2) {
        probe->invalid = true;
        return;
    }
    uint32_t *event = probe->events[probe->event_count++];
    event[0] = kind;
    event[1] = contact->flags;
    memset(event + 2, 0, 16 * sizeof(uint32_t));
    if (previous) {
        memcpy(event + 2, previous, sizeof(*previous));
    }
    if (kind == 1 && probe->mode == 2) {
        contact->flags &= ~(uint32_t)AB_CONTACT_ENABLED;
    }
}

static void begin(void *context, AbContactState *contact) {
    record_event(context, 1, contact, NULL);
}

static void end(void *context, AbContactState *contact) {
    record_event(context, 2, contact, NULL);
}

static void pre_solve(void *context, AbContactState *contact,
                      const AbManifold *previous) {
    record_event(context, 3, contact, previous);
}

static bool edge_checks(void) {
    AbContactState contact = {.manifold = {.point_count = 2}};
    CHECK(ab_contact_state_initialize(&contact, 4, 9, -1, 2));
    CHECK(contact.flags == AB_CONTACT_ENABLED && contact.friction == 6 &&
          contact.restitution == 2 && contact.manifold.point_count == 0);
    AbContactState unchanged = contact;
    CHECK(!ab_contact_state_initialize(&contact, -1, 1, 0, 0));
    CHECK(!ab_contact_state_initialize(&contact, FLT_MAX, FLT_MAX, 0, 0));
    CHECK(!ab_contact_state_initialize(&contact, 1, 1, NAN, 0));
    CHECK(memcmp(&contact, &unchanged, sizeof(contact)) == 0);
    AbBodyMotion first = {.sleep_time = 7}, second = {.sleep_time = 8};
    ContactProbe probe = {.overlap = true};
    AbContactEvaluator evaluator = {&probe, evaluate, overlap};
    AbContactListener listener = {&probe, begin, end, pre_solve};
    CHECK(ab_contact_state_update(&contact, &first, &second, true, &evaluator,
                                  &listener));
    CHECK(contact.manifold.point_count == 0 && probe.event_count == 1 &&
          probe.events[0][0] == 1 && !first.awake && !second.awake &&
          first.sleep_time == 7 && second.sleep_time == 8);
    probe.overlap = false;
    CHECK(ab_contact_state_update(&contact, &first, &second, true, &evaluator,
                                  &listener));
    CHECK(probe.event_count == 2 && probe.events[1][0] == 2 && !probe.invalid);
    probe = (ContactProbe){.mode = 2, .evaluation = {.point_count = 2}};
    contact.manifold.point_count = 2;
    contact.manifold.points[0] = (AbContactPoint){.feature_id = 5, .normal_impulse = 3};
    contact.manifold.points[1] = (AbContactPoint){.feature_id = 5, .normal_impulse = 9};
    probe.evaluation.points[0].feature_id = 5;
    probe.evaluation.points[1].feature_id = 7;
    CHECK(ab_contact_state_update(&contact, &first, &second, false, &evaluator,
                                  &listener));
    CHECK(contact.manifold.points[0].normal_impulse == 3 &&
          contact.manifold.points[1].normal_impulse == 0);
    CHECK(first.awake && second.awake && first.sleep_time == 0 &&
          second.sleep_time == 0 && !(contact.flags & AB_CONTACT_ENABLED));
    CHECK(probe.event_count == 2 && probe.events[0][0] == 1 &&
          probe.events[1][0] == 3 && !(probe.events[1][1] & AB_CONTACT_ENABLED));
    unchanged = contact;
    CHECK(!ab_contact_state_update(&contact, NULL, &second, false, &evaluator, NULL));
    CHECK(!ab_contact_state_update(&contact, &first, &second, false, NULL, NULL));
    CHECK(memcmp(&contact, &unchanged, sizeof(contact)) == 0);
    probe = (ContactProbe){.evaluation = {.point_count = 3}};
    CHECK(!ab_contact_state_update(&contact, &first, &second, false, &evaluator, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t sample, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Contact sample %zu %s field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbContactState read_contact(const uint32_t words[20]) {
    AbContactState contact = {.flags = words[0],
                              .friction = from_bits(words[17]),
                              .restitution = from_bits(words[18]),
                              .tangent_speed = from_bits(words[19])};
    memcpy(&contact.manifold, words + 1, sizeof(contact.manifold));
    return contact;
}

static void write_contact(const AbContactState *contact, uint32_t words[20]) {
    words[0] = contact->flags;
    memcpy(words + 1, &contact->manifold, sizeof(contact->manifold));
    words[17] = to_bits(contact->friction);
    words[18] = to_bits(contact->restitution);
    words[19] = to_bits(contact->tangent_speed);
}

static bool compare_sample(FILE *file, size_t sample, size_t *events) {
    uint32_t materials[4], initial[20], expected[20], actual[20];
    CHECK(read_words(file, materials, 4) && read_words(file, initial, 20) &&
          read_words(file, expected, 20));
    AbContactState contact = read_contact(initial);
    CHECK(ab_contact_state_initialize(&contact, from_bits(materials[0]),
                                      from_bits(materials[1]), from_bits(materials[2]),
                                      from_bits(materials[3])));
    write_contact(&contact, actual);
    CHECK(compare_words(actual, expected, 20, sample, "constructor"));
    uint32_t control[8], old[16], evaluation[16], expected_bodies[4], event_count = 0;
    CHECK(read_words(file, control, 8) && read_words(file, old, 16) &&
          read_words(file, evaluation, 16) && read_words(file, expected, 20) &&
          read_words(file, expected_bodies, 4) && read_words(file, &event_count, 1));
    CHECK(control[0] <= 1 && control[1] <= 2 && control[7] <= 1 && event_count <= 2);
    contact.flags = control[2];
    memcpy(&contact.manifold, old, sizeof(contact.manifold));
    ContactProbe probe = {.mode = control[1], .overlap = control[7] != 0};
    memcpy(&probe.evaluation, evaluation, sizeof(probe.evaluation));
    AbContactEvaluator evaluator = {&probe, evaluate, overlap};
    AbContactListener listener = {&probe, begin, end, pre_solve};
    AbBodyMotion first = {.awake = (control[3] & 2) != 0,
                          .sleep_time = from_bits(control[5])};
    AbBodyMotion second = {.awake = (control[4] & 2) != 0,
                           .sleep_time = from_bits(control[6])};
    CHECK(ab_contact_state_update(&contact, &first, &second, control[0] != 0,
                                  &evaluator, control[1] ? &listener : NULL));
    write_contact(&contact, actual);
    CHECK(compare_words(actual, expected, 20, sample, "update"));
    const uint32_t bodies[] = {
        (control[3] & ~UINT32_C(2)) | (first.awake ? 2u : 0u),
        (control[4] & ~UINT32_C(2)) | (second.awake ? 2u : 0u),
        to_bits(first.sleep_time),
        to_bits(second.sleep_time),
    };
    CHECK(compare_words(bodies, expected_bodies, 4, sample, "bodies"));
    CHECK(probe.event_count == event_count && !probe.invalid);
    for (size_t event = 0; event < event_count; ++event) {
        uint32_t expected_event[18];
        CHECK(read_words(file, expected_event, 18));
        CHECK(
            compare_words(probe.events[event], expected_event, 18, sample, "callback"));
    }
    *events += event_count;
    return true;
}

static bool native_checks(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_CONTACT_STATE_V1 4096\n") == 0);
    size_t events = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(compare_sample(file, sample, &events));
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB contact state reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 contact constructors/updates and %zu callbacks match.\n", events);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_contact_state_tests [NATIVE_CONTACT_STATE_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
