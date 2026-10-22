#include "ab/game_contact.h"

#include "contact_glue_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Contact glue check failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t calls, names[2], fail_at;
    uint32_t flags, callbacks;
    AbDataValue response;
} Lookup;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool body_lookup(void *context, AbSpan name, size_t *body, AbError *error) {
    Lookup *lookup = context;
    CHECK(lookup->calls < 2);
    if (lookup->calls == lookup->fail_at) {
        return ab_fail(error, "Cannot resolve contact body");
    }
    for (uint32_t index = 0; index < 3; ++index) {
        if (name.size == strlen(contact_glue_names[index]) &&
            memcmp(name.data, contact_glue_names[index], name.size) == 0) {
            lookup->names[lookup->calls++] = index;
            *body = index == 2 ? SIZE_MAX : index;
            return true;
        }
    }
    return false;
}

static bool contact_callback(void *context, AbGameContactCall *call, AbError *error) {
    (void)error;
    Lookup *lookup = context;
    CHECK(call->first.size == 5 && memcmp(call->first.data, "first", 5) == 0 &&
          call->second.size == 6 && memcmp(call->second.data, "second", 6) == 0 &&
          !call->response.should_disable_contact &&
          call->response.new_collision_parameters.kind == AB_DATA_NIL &&
          call->response.glue_contact_pair.kind == AB_DATA_NIL);
    ++lookup->callbacks;
    call->response.should_disable_contact = (lookup->flags & 4u) != 0;
    call->response.glue_contact_pair = lookup->response;
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    return fgetc(file) == '\n';
}

static AbDataValue response_value(const uint32_t input[5], AbDataTable *table,
                                  AbDataEntry entries[2]) {
    entries[0] = (AbDataEntry){{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                               text(contact_glue_names[input[3]])};
    entries[1] = (AbDataEntry){{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)},
                               text(contact_glue_names[input[4]])};
    *table = (AbDataTable){.entries = entries, .count = 2, .capacity = 2};
    return input[1] == 0 ? (AbDataValue){0}
           : input[1] <= 2
               ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = input[1] == 2}
           : input[1] == 3 ? (AbDataValue){.kind = AB_DATA_NUMBER}
           : input[1] == 4 ? text("invalid")
                           : (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool native_case(FILE *file, uint32_t sample, size_t *appended,
                        size_t *missing) {
    uint32_t input[5], generated[5], expected[3], expected_pairs[8];
    contact_glue_case(sample, generated);
    CHECK(read_words(file, input, 5) && memcmp(input, generated, sizeof(input)) == 0 &&
          read_words(file, expected, 3) && read_words(file, expected_pairs, 8));
    AbClassicGluePair pairs[4];
    for (size_t index = 0; index < 4; ++index) {
        pairs[index] = (AbClassicGluePair){index % 2, (index + 1) % 2};
    }
    AbClassicGlueQueue queue = {pairs, input[2], 4};
    AbDataEntry entries[2];
    AbDataTable table;
    AbDataValue response = response_value(input, &table, entries);
    Lookup lookup = {.fail_at = UINT32_MAX, .flags = input[0], .response = response};
    AbGameContactObject first = {text("first").string, (input[0] & 64u) != 0};
    AbGameContactObject second = {text("second").string, (input[0] & 128u) != 0};
    AbContactState contact = {.flags = 7};
    AbWorldManifold geometry = {.normal = {1, 0}, .points = {{1, 0}}};
    AbGameContactInput request = {.first = input[0] & 16u ? &first : NULL,
                                  .second = input[0] & 32u ? &second : NULL,
                                  .manifold = &geometry,
                                  .contact = &contact,
                                  .glue = &queue,
                                  .callbacks_disabled = (input[0] & 8u) != 0};
    CHECK(ab_game_contact_begin(
        &request, (AbGameContactHost){&lookup, contact_callback, NULL, body_lookup},
        NULL));
    bool called = lookup.callbacks != 0;
    uint32_t actual[] = {contact.flags, lookup.callbacks, (uint32_t)queue.count};
    uint32_t actual_pairs[8];
    for (size_t index = 0; index < 8; ++index) {
        actual_pairs[index] = UINT32_MAX;
    }
    for (size_t index = 0; index < queue.count; ++index) {
        actual_pairs[index * 2] = pairs[index].first_body == SIZE_MAX
                                      ? 0
                                      : (uint32_t)pairs[index].first_body + 1;
        actual_pairs[index * 2 + 1] = pairs[index].second_body == SIZE_MAX
                                          ? 0
                                          : (uint32_t)pairs[index].second_body + 1;
    }
    if (memcmp(actual, expected, sizeof(actual)) ||
        memcmp(actual_pairs, expected_pairs, sizeof(actual_pairs))) {
        fprintf(stderr, "Native contact glue mismatch in sample %" PRIu32 "\n", sample);
        return false;
    }
    bool added = called && input[1] >= 5;
    CHECK(lookup.calls == (added ? 2u : 0u));
    if (added) {
        CHECK(lookup.names[0] == input[3] && lookup.names[1] == input[4]);
        ++*appended;
        *missing += input[3] == 2 || input[4] == 2;
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_CONTACT_GLUE_V1 4096\n") == 0);
    size_t appended = 0, missing = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        CHECK(native_case(file, sample, &appended, &missing));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB contact glue reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "Matched 4,096 original contact responses: %zu appended glue pairs, "
        "%zu with absent endpoints; queue prefix, pair order and duplicates retained\n",
        appended, missing);
    return true;
}

static bool edge_checks(void) {
    const uint32_t input[] = {240, 5, 0, 0, 1};
    AbDataEntry entries[2];
    AbDataTable table;
    AbDataValue response = response_value(input, &table, entries);
    AbClassicGluePair pairs[2] = {{3, 4}, {5, 6}}, before[2];
    memcpy(before, pairs, sizeof(pairs));
    AbClassicGlueQueue queue = {pairs, 1, 2};
    Lookup lookup = {.fail_at = 1};
    CHECK(
        !ab_classic_glue_queue_contact(&queue, response, body_lookup, &lookup, NULL) &&
        lookup.calls == 1 && queue.count == 1 &&
        memcmp(pairs, before, sizeof(pairs)) == 0);
    lookup = (Lookup){.fail_at = UINT32_MAX};
    entries[1].value.kind = AB_DATA_NUMBER;
    CHECK(
        !ab_classic_glue_queue_contact(&queue, response, body_lookup, &lookup, NULL) &&
        !lookup.calls && queue.count == 1);
    entries[1].value = text("second");
    queue.count = 2;
    CHECK(
        !ab_classic_glue_queue_contact(&queue, response, body_lookup, &lookup, NULL) &&
        !lookup.calls && queue.count == 2);
    CHECK(ab_classic_glue_queue_contact(&queue, (AbDataValue){0}, NULL, NULL, NULL));
    queue.count = 1;
    CHECK(ab_classic_glue_queue_contact(&queue, response, body_lookup, &lookup, NULL) &&
          queue.count == 2 && pairs[0].first_body == 3 && pairs[0].second_body == 4 &&
          pairs[1].first_body == 0 && pairs[1].second_body == 1);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    FILE *file = argc == 2 ? fopen(argv[1], "r") : NULL;
    if (argc == 2 && !file) {
        return 1;
    }
    bool okay = file ? native_checks(file) : edge_checks();
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
