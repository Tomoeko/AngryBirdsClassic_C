#include "ab/asset_groups.h"

#include "asset_group_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Asset group check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbAssetGroups *state;
    AbSpan profile;
    AbDataEntry fields[4][2], resources[4], group;
    AbDataTable rows[4], resource_table, groups;
    uint32_t events[8][5];
    size_t count;
    AbAssetOperation fail_at;
} AssetFixture;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue numeric(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(result.bits));
    return result;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static uint32_t name_id(AbSpan name) {
    for (uint32_t index = 0; index < 5; ++index) {
        if (name.size == strlen(asset_group_names[index]) &&
            (!name.size ||
             memcmp(name.data, asset_group_names[index], name.size) == 0)) {
            return index;
        }
    }
    return UINT32_MAX;
}

static uint32_t hash_span(AbSpan span) {
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < span.size; ++index) {
        hash = (hash ^ span.data[index]) * UINT32_C(16777619);
    }
    return hash;
}

static bool asset_host(void *context, AbAssetCall *call, AbError *error) {
    AssetFixture *fixture = context;
    CHECK(fixture->count < 8 && call->operation >= AB_ASSET_SELECT_PROFILE &&
          call->operation <= AB_ASSET_RELEASE_COMPOSITES && fixture->state->count <= 7);
    char path[256];
    size_t length;
    AbSpan argument;
    if (call->operation == AB_ASSET_SELECT_PROFILE) {
        argument = call->group;
    } else if (call->operation == AB_ASSET_LOAD_LIST) {
        argument = call->profile;
    } else {
        CHECK(ab_asset_resource_path(call, path, sizeof(path), &length, error));
        argument = (AbSpan){(const uint8_t *)path, length};
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = call->operation;
    event[1] = hash_span(argument);
    event[2] = (uint32_t)argument.size;
    event[3] = (uint32_t)fixture->state->count;
    event[4] = 0;
    for (size_t index = 0; index < fixture->state->count; ++index) {
        uint32_t id = name_id(fixture->state->loaded[index]);
        CHECK(id != UINT32_MAX);
        event[4] |= (id + 1) << (index * 3);
    }
    if (fixture->fail_at == call->operation) {
        return ab_fail(error, "Controlled asset host failure");
    }
    if (call->operation == AB_ASSET_SELECT_PROFILE) {
        call->profile = fixture->profile;
    } else if (call->operation == AB_ASSET_LOAD_LIST) {
        call->groups = &fixture->groups;
    }
    return true;
}

static void prepare_fixture(AssetFixture *fixture, AbAssetGroups *state,
                            const uint32_t input[7], const uint32_t kinds[4]) {
    *fixture = (AssetFixture){.state = state,
                              .profile = text(asset_group_profiles[input[3]]).string};
    for (size_t index = 0; index < 4; ++index) {
        fixture->fields[index][0] =
            (AbDataEntry){numeric(1), text(asset_group_files[index])};
        fixture->fields[index][1] =
            (AbDataEntry){numeric(2), numeric((float)kinds[index])};
        fixture->rows[index] = (AbDataTable){fixture->fields[index], 2, 2, NULL};
        fixture->resources[index] =
            (AbDataEntry){numeric((float)index + 1), table(fixture->rows + index)};
    }
    fixture->resource_table = (AbDataTable){fixture->resources, input[5], 4, NULL};
    fixture->group = (AbDataEntry){text(asset_group_names[input[2]]),
                                   table(&fixture->resource_table)};
    fixture->groups = (AbDataTable){&fixture->group, input[6], 1, NULL};
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_case(FILE *file, uint32_t sample) {
    uint32_t input[7], loaded[6], kinds[4], actual_input[7], actual_loaded[6],
        actual_kinds[4];
    asset_group_case(sample, input, loaded, kinds);
    CHECK(read_words(file, actual_input, 7) && read_words(file, actual_loaded, 6) &&
          read_words(file, actual_kinds, 4) &&
          memcmp(input, actual_input, sizeof(input)) == 0 &&
          memcmp(loaded, actual_loaded, sizeof(loaded)) == 0 &&
          memcmp(kinds, actual_kinds, sizeof(kinds)) == 0);
    AbSpan names[7], scratch[7];
    AbAssetGroups state = {names, input[4], 7, scratch, 7, false};
    for (size_t index = 0; index < state.count; ++index) {
        names[index] = text(asset_group_names[loaded[index]]).string;
    }
    AssetFixture fixture;
    prepare_fixture(&fixture, &state, input, kinds);
    CHECK(ab_asset_group_manage(&state, text(asset_group_names[input[2]]).string,
                                input[1] != 0,
                                text(sample & 256u ? "/data/images" : "images").string,
                                asset_host, &fixture, NULL));
    uint32_t output[2], final[7], actual[7];
    CHECK(read_words(file, output, 2) && read_words(file, final, 7) &&
          output[0] == fixture.count && output[1] == state.count && !state.failed);
    for (size_t index = 0; index < 7; ++index) {
        actual[index] = index < state.count ? name_id(state.loaded[index]) : UINT32_MAX;
    }
    CHECK(memcmp(final, actual, sizeof(final)) == 0);
    for (size_t index = 0; index < fixture.count; ++index) {
        uint32_t event[5];
        CHECK(read_words(file, event, 5));
        for (size_t field = 0; field < 5; ++field) {
            if (event[field] != fixture.events[index][field]) {
                fprintf(stderr,
                        "Native asset group %" PRIu32 " event %zu field %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        sample, index, field, fixture.events[index][field],
                        event[field]);
                return false;
            }
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_ASSET_GROUP_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        CHECK(native_case(file, sample));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB asset group reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 4,096 original manageAssets calls: ordered profile/list/resource "
         "dispatch, pre-call loaded membership, repeated loads, forward removals "
         "and ignored resource kinds");
    return true;
}

static bool edge_checks(void) {
    AbSpan names[7] = {text("alpha").string, text("alpha").string, text("beta").string};
    AbSpan scratch[7];
    AbAssetGroups state = {names, 3, 7, scratch, 7, false};
    const uint32_t input[] = {0, 0, 0, 0, 3, 0, 1}, kinds[] = {0, 1, 2, 3};
    AssetFixture fixture;
    prepare_fixture(&fixture, &state, input, kinds);
    CHECK(ab_asset_group_manage(&state, text("alpha").string, false,
                                text("images").string, asset_host, &fixture, NULL) &&
          state.count == 2 && name_id(names[0]) == 0 && name_id(names[1]) == 1);
    state.count = 0;
    fixture.count = 0;
    fixture.resource_table.count = 2;
    fixture.fail_at = AB_ASSET_CREATE_COMPOSITES;
    AbError error = {0};
    CHECK(!ab_asset_group_manage(&state, text("alpha").string, true,
                                 text("images").string, asset_host, &fixture, &error) &&
          state.failed && state.count == 1 && fixture.count == 4 &&
          strstr(error.message, "Controlled asset host failure"));
    CHECK(!ab_asset_group_manage(&state, text("alpha").string, true,
                                 text("images").string, asset_host, &fixture, NULL) &&
          fixture.count == 4);
    state.failed = false;
    state.count = 0;
    fixture.count = 0;
    fixture.fail_at = 0;
    fixture.resource_table.count = 1;
    fixture.fields[0][1].value = numeric(NAN);
    fixture.fields[0][0].value.kind = AB_DATA_BOOLEAN;
    CHECK(ab_asset_group_manage(&state, text("alpha").string, true,
                                text("images").string, asset_host, &fixture, NULL) &&
          state.count == 1 && fixture.count == 2);
    char path[32] = "unchanged";
    size_t length = 99;
    AbAssetCall call = {.image_root = text("root").string,
                        .profile = text("profile").string,
                        .filename = text("file").string};
    CHECK(!ab_asset_resource_path(&call, path, 3, &length, NULL) && length == 99 &&
          strcmp(path, "unchanged") == 0);
    CHECK(ab_asset_resource_path(&call, path, sizeof(path), &length, NULL) &&
          strcmp(path, "root/profile/file") == 0 && length == strlen(path));
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
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
