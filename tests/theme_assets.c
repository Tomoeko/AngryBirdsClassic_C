#include "ab/theme_assets.h"
#include "theme_asset_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme assets failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbThemeAssets state;
    AbDataTable old, target;
    AbDataEntry old_entries[8], target_entries[8];
    AbSpan scratch[16];
    const AbDataTable *target_identity;
    uint32_t output[69], fail;
    bool check_reentry;
} Fixture;

static AbDataValue number(size_t index) {
    float real = (float)index;
    AbDataValue value = {.kind = AB_DATA_NUMBER};
    memcpy(&value.bits, &real, sizeof(real));
    return value;
}

static AbDataValue name_value(uint32_t index) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)theme_asset_names[index],
                                    strlen(theme_asset_names[index])}};
}

static uint32_t name_id(AbSpan name) {
    for (uint32_t index = 0; index < 6; ++index) {
        AbSpan other = name_value(index).string;
        if (name.size == other.size &&
            (!name.size || memcmp(name.data, other.data, name.size) == 0)) {
            return index;
        }
    }
    return UINT32_MAX;
}

static uint32_t identity(const Fixture *fixture) {
    uint32_t flags = (fixture->state.current == &fixture->old ? 1u : 0u) |
                     (fixture->state.current == fixture->target_identity ? 2u : 0u);
    return flags ? flags : 4;
}

static bool visit(void *context, AbSpan group, bool loading, AbError *error) {
    Fixture *fixture = context;
    CHECK(fixture->output[1] < 13 && name_id(group) < 6 && fixture->state.busy);
    size_t offset = 30 + fixture->output[1] * 3;
    fixture->output[offset] = loading;
    fixture->output[offset + 1] = name_id(group);
    fixture->output[offset + 2] = identity(fixture);
    ++fixture->output[1];
    if (fixture->check_reentry) {
        CHECK(!ab_theme_assets_transition(&fixture->state, name_value(0), false, visit,
                                          fixture, NULL));
    }
    return !fixture->fail || fixture->output[1] != fixture->fail ||
           ab_fail(error, "Controlled theme asset boundary failure");
}

static void table(AbDataTable *output, AbDataEntry *entries, const uint32_t *names,
                  uint32_t count) {
    *output = (AbDataTable){entries, count, 8, NULL};
    for (size_t index = 0; index < count; ++index) {
        entries[index] = (AbDataEntry){number(index + 1), name_value(names[index])};
    }
}

static void members(const AbDataTable *source, uint32_t *output) {
    for (size_t index = 0; index < source->count; ++index) {
        output[index] = name_id(ab_data_get(source, number(index + 1))->string);
    }
}

static bool run_case(const uint32_t input[18], uint32_t output[69]) {
    Fixture fixture = {.fail = input[5]};
    table(&fixture.old, fixture.old_entries, input + 6, input[3]);
    table(&fixture.target, fixture.target_entries, input + 12, input[4]);
    AbDataTable *target = input[2] ? &fixture.old : &fixture.target;
    fixture.target_identity = input[0] ? NULL : target;
    for (size_t index = 0; index < 69; ++index) {
        fixture.output[index] = index >= 6 ? UINT32_MAX : 0;
    }
    CHECK(ab_theme_assets_init(&fixture.old, fixture.scratch, 8, &fixture.state, NULL));
    AbDataValue graphic = input[0]
                              ? name_value(input[12])
                              : (AbDataValue){.kind = AB_DATA_TABLE, .table = target};
    fixture.output[0] = ab_theme_assets_transition(
        &fixture.state, graphic, input[1] >= 1 && input[1] <= 3, visit, &fixture, NULL);
    CHECK(!fixture.state.busy && fixture.state.failed == !fixture.output[0]);
    fixture.output[2] = identity(&fixture);
    fixture.output[3] = (uint32_t)fixture.state.current->count;
    fixture.output[4] = input[0] ? 1 : (uint32_t)target->count;
    fixture.output[5] = (uint32_t)fixture.old.count;
    members(fixture.state.current, fixture.output + 6);
    if (input[0]) {
        fixture.output[14] = input[12];
    } else {
        members(target, fixture.output + 14);
    }
    members(&fixture.old, fixture.output + 22);
    memcpy(output, fixture.output, sizeof(fixture.output));
    if (fixture.state.failed) {
        CHECK(!ab_theme_assets_transition(&fixture.state, graphic, false, visit,
                                          &fixture, NULL) &&
              memcmp(output, fixture.output, sizeof(fixture.output)) == 0);
    }
    return true;
}

static bool edges(void) {
    Fixture fixture = {0};
    fixture.old = (AbDataTable){fixture.old_entries, 0, 8, NULL};
    CHECK(
        !ab_theme_assets_init(NULL, fixture.scratch, 8, &fixture.state, NULL) &&
        !ab_theme_assets_init(&fixture.old, NULL, 8, &fixture.state, NULL) &&
        !ab_theme_assets_init(&fixture.old, fixture.scratch, 0, &fixture.state, NULL) &&
        !ab_theme_assets_init(&fixture.old, fixture.scratch, SIZE_MAX, &fixture.state,
                              NULL) &&
        ab_theme_assets_init(&fixture.old, fixture.scratch, 8, &fixture.state, NULL));
    CHECK(!ab_theme_assets_transition(&fixture.state, (AbDataValue){0}, false, visit,
                                      &fixture, NULL) &&
          !ab_theme_assets_transition(
              &fixture.state,
              (AbDataValue){.kind = AB_DATA_STRING, .string = {NULL, 1}}, false, visit,
              &fixture, NULL) &&
          !fixture.state.failed && fixture.state.current == &fixture.old &&
          !fixture.output[1]);
    fixture.check_reentry = true;
    CHECK(ab_theme_assets_transition(&fixture.state, name_value(0), false, visit,
                                     &fixture, NULL) &&
          ab_theme_assets_transition(&fixture.state, name_value(1), false, visit,
                                     &fixture, NULL) &&
          ab_theme_assets_transition(&fixture.state, name_value(0), false, visit,
                                     &fixture, NULL) &&
          fixture.output[1] == 5 && fixture.state.current->count == 1 &&
          name_id(ab_data_get(fixture.state.current, number(1))->string) == 0);
    CHECK(ab_theme_assets_init(&fixture.old, fixture.scratch, 8, &fixture.state, NULL));
    fixture.output[1] = 0;
    AbDataValue alias = {.kind = AB_DATA_TABLE, .table = &fixture.old};
    CHECK(ab_theme_assets_transition(&fixture.state, alias, true, visit, &fixture,
                                     NULL) &&
          ab_theme_assets_transition(&fixture.state, alias, true, visit, &fixture,
                                     NULL) &&
          fixture.old.count == 2 && !fixture.output[1]);
    fixture.old.capacity = 2;
    CHECK(!ab_theme_assets_transition(&fixture.state, alias, true, visit, &fixture,
                                      NULL) &&
          fixture.old.count == 2 && !fixture.output[1] && !fixture.state.failed);
    fixture.old_entries[1].key = number(3);
    CHECK(!ab_theme_assets_transition(&fixture.state, name_value(0), false, visit,
                                      &fixture, NULL) &&
          !fixture.output[1]);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, output + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static bool native(FILE *file) {
    char line[80];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_THEME_ASSET_V1 4096\n") == 0);
    size_t events = 0, failures = 0, aliases = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[18], generated[18], expected[69], actual[69];
        theme_asset_case(sample, generated);
        CHECK(read_words(file, input, 18) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 69) && run_case(input, actual));
        for (size_t word = 0; word < 69; ++word) {
            if (actual[word] != expected[word]) {
                fprintf(stderr,
                        "Native theme case %" PRIu32 " word %zu: %08" PRIx32
                        "/%08" PRIx32 "\n",
                        sample, word, actual[word], expected[word]);
                return false;
            }
        }
        events += actual[1];
        failures += !actual[0];
        aliases += !input[0] && input[2];
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB theme asset reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 4096 native theme transitions, %zu callbacks, %zu controlled "
           "failures and %zu table aliases (282624 output words).\n",
           events, failures, aliases);
    return true;
}

#include "theme_asset_resources.h"

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--apk") == 0) {
        return theme_resource_checks(argv[2]) ? 0 : 1;
    }
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_theme_asset_tests [NATIVE_REFERENCE] | --apk APK\n");
        return 2;
    }
    if (!edges()) {
        return 1;
    }
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    bool okay = argc == 1 || (file && native(file));
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
