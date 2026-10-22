#include "ab/asset_bundles.h"
#include "ab/asset_groups.h"
#include "asset_bundle_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Asset bundle check failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataEntry requests[5], profiles[5], members[5][3];
    AbDataTable request_table, profile_table, bundles[5];
    uint32_t events[15][2];
    size_t count, fail_at;
} Fixture;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(uint32_t index) {
    float value = (float)index;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(result.bits));
    return result;
}

static void prepare(Fixture *fixture, const uint32_t input[28]) {
    *fixture = (Fixture){.fail_at = SIZE_MAX};
    fixture->request_table = (AbDataTable){fixture->requests, 0, 5, NULL};
    fixture->profile_table = (AbDataTable){fixture->profiles, 0, 5, NULL};
    for (uint32_t index = 0; index < 5; ++index) {
        if (input[2] & (1u << index)) {
            fixture->requests[fixture->request_table.count++] =
                (AbDataEntry){number(index + 1), text(bundle_names[input[3 + index]])};
        }
        if (!(input[1] & (1u << index))) {
            continue;
        }
        AbDataTable *bundle = fixture->bundles + index;
        *bundle = (AbDataTable){fixture->members[index], 0, 3, NULL};
        for (uint32_t item = 0; item < 3; ++item) {
            if (input[8 + index] & (1u << item)) {
                bundle->entries[bundle->count++] = (AbDataEntry){
                    number(item + 1), text(bundle_names[input[13 + index * 3 + item]])};
            }
        }
        fixture->profiles[fixture->profile_table.count++] = (AbDataEntry){
            text(bundle_names[index]), {.kind = AB_DATA_TABLE, .table = bundle}};
    }
}

static bool visit(void *context, AbSpan group, bool loading, AbError *error) {
    Fixture *fixture = context;
    CHECK(fixture->count < 15);
    uint32_t id = 5;
    for (uint32_t index = 0; index < 5; ++index) {
        if (group.size == strlen(bundle_names[index]) &&
            (!group.size || memcmp(group.data, bundle_names[index], group.size) == 0)) {
            id = index;
        }
    }
    CHECK(id < 5);
    size_t event = fixture->count++;
    fixture->events[event][0] = id;
    fixture->events[event][1] = loading;
    return event != fixture->fail_at ||
           ab_fail(error, "Controlled bundle visit failure");
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, output + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static bool native_checks(FILE *file) {
    char line[100];
    size_t visits = 0;
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_ASSET_BUNDLE_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[28], generated[28], counts[2];
        asset_bundle_case(sample, generated);
        CHECK(read_words(file, input, 28) &&
              memcmp(input, generated, sizeof(input)) == 0);
        Fixture fixture;
        prepare(&fixture, input);
        CHECK(ab_asset_bundle_dispatch(&fixture.request_table, &fixture.profile_table,
                                       input[0] != 0, visit, &fixture, NULL));
        CHECK(read_words(file, counts, 2) && counts[0] == 1 &&
              counts[1] == fixture.count);
        for (size_t event = 0; event < fixture.count; ++event) {
            uint32_t expected[2];
            CHECK(read_words(file, expected, 2));
            if (memcmp(expected, fixture.events[event], sizeof(expected)) != 0) {
                fprintf(stderr,
                        "Original bundle sample %" PRIu32 " event %zu differs\n",
                        sample, event);
                return false;
            }
        }
        visits += fixture.count;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB asset bundle reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 4,096 original bundle expansions and %zu ordered group visits "
           "after the original requireFile boundary.\n",
           visits);
    return true;
}

static bool empty_resources(void *context, AbAssetCall *call, AbError *error) {
    (void)error;
    size_t *calls = context;
    ++*calls;
    static const AbDataTable groups = {0};
    if (call->operation == AB_ASSET_SELECT_PROFILE) {
        call->profile = text("profile").string;
    } else {
        CHECK(call->operation == AB_ASSET_LOAD_LIST);
        call->groups = &groups;
    }
    return true;
}

static bool edge_checks(void) {
    uint32_t input[28] = {0};
    input[1] = 1;
    input[2] = 3;
    input[4] = 1;
    Fixture fixture;
    prepare(&fixture, input);
    CHECK(ab_asset_bundle_dispatch(&fixture.request_table, &fixture.profile_table,
                                   false, visit, &fixture, NULL) &&
          fixture.count == 1 && fixture.events[0][0] == 1 && !fixture.events[0][1]);
    AbSpan names[5], scratch[5];
    AbAssetGroups state = {names, 0, 5, scratch, 5, false};
    size_t calls = 0;
    CHECK(ab_asset_group_manage_many(
              &state, &fixture.request_table, &fixture.profile_table, true,
              text("images").string, empty_resources, &calls, NULL) &&
          state.count == 1 && calls == 2);
    CHECK(ab_asset_group_manage_many(
              &state, &fixture.request_table, &fixture.profile_table, false,
              text("images").string, empty_resources, &calls, NULL) &&
          !state.count && calls == 4);
    fixture.profiles[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    fixture.count = 0;
    CHECK(!ab_asset_bundle_dispatch(&fixture.request_table, &fixture.profile_table,
                                    true, visit, &fixture, NULL) &&
          !fixture.count);
    fixture.profile_table.count = 0;
    fixture.requests[1].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(!ab_asset_bundle_dispatch(&fixture.request_table, &fixture.profile_table,
                                    true, visit, &fixture, NULL) &&
          fixture.count == 1);
    fixture.requests[1].value = text("beta");
    fixture.count = fixture.fail_at = 0;
    AbError error = {0};
    CHECK(!ab_asset_bundle_dispatch(&fixture.request_table, &fixture.profile_table,
                                    true, visit, &fixture, &error) &&
          fixture.count == 1 &&
          strstr(error.message, "Controlled bundle visit failure"));
    fixture.request_table.count = 6;
    CHECK(!ab_asset_group_manage_many(
              &state, &fixture.request_table, &fixture.profile_table, true,
              text("images").string, empty_resources, &calls, NULL) &&
          state.failed && calls == 4);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
