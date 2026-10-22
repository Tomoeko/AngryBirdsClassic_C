#include "ab/ui_localization.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI localization failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const groups[] = {"", "TEXTS_BASIC", "GROUP_A", "GROUP_B",
                                     "GROUP_C"};
static const char *const texts[] = {"", "id", "same", "translated", "value"};

typedef struct {
    AbUiLocalizationCache *cache;
    uint32_t responses[5][5], calls[8][5];
    size_t count;
    bool fail;
} Boundaries;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static uint32_t identifier(AbSpan value, const char *const choices[5]) {
    for (uint32_t index = 0; index < 5; ++index) {
        AbSpan candidate = span(choices[index]);
        if (candidate.size == value.size &&
            (!candidate.size ||
             memcmp(candidate.data, value.data, candidate.size) == 0)) {
            return index;
        }
    }
    return UINT32_MAX;
}

static bool lookup(void *context, AbSpan group, AbSpan requested, AbSpan *output,
                   AbError *error) {
    (void)error;
    Boundaries *input = context;
    uint32_t group_id = identifier(group, groups), id = identifier(requested, texts);
    CHECK(input->count < 8 && group_id < 5 && id < 5 &&
          input->responses[group_id][id] < 5);
    uint32_t *call = input->calls[input->count++];
    call[0] = group_id;
    call[1] = id;
    call[2] = input->responses[group_id][id];
    call[3] = input->cache->present;
    call[4] = input->cache->present ? identifier(input->cache->group, groups) : 0;
    *output = span(texts[call[2]]);
    return !input->fail;
}

static bool edge_checks(void) {
    AbUiLocalizationCache cache = {0};
    Boundaries calls = {.cache = &cache};
    for (uint32_t group = 0; group < 5; ++group) {
        for (uint32_t id = 0; id < 5; ++id) {
            calls.responses[group][id] = id;
        }
    }
    AbSpan profile[] = {span(groups[1]), span(groups[2]), span(groups[2])};
    AbUiLocalizationSource source = {profile, 3, &calls, lookup};
    AbSpan output;
    CHECK(ab_ui_localization_string(&source, span(texts[1]), NULL, &output, NULL) &&
          identifier(output, texts) == 1 && calls.count == 4 &&
          calls.calls[0][0] == 1 && calls.calls[1][0] == 2 && calls.calls[2][0] == 2 &&
          calls.calls[3][0] == 1);
    cache = (AbUiLocalizationCache){span(groups[2]), true};
    calls.count = 0;
    CHECK(ab_ui_localization_string(&source, span(texts[1]), &cache, &output, NULL) &&
          calls.count == 3 && calls.calls[0][0] == 2 && calls.calls[1][0] == 1 &&
          calls.calls[2][0] == 1 && identifier(cache.group, groups) == 2);
    calls.responses[2][1] = 0;
    cache.present = false;
    calls.count = 0;
    CHECK(ab_ui_localization_string(&source, span(texts[1]), &cache, &output, NULL) &&
          !output.size && calls.count == 2 && cache.present &&
          identifier(cache.group, groups) == 2);
    calls.count = 0;
    CHECK(ab_ui_localization_string(&source, span(texts[1]), &cache, &output, NULL) &&
          !output.size && calls.count == 1);
    calls.count = 0;
    calls.responses[1][0] = 4;
    CHECK(ab_ui_localization_string(&source, span(texts[0]), &cache, &output, NULL) &&
          identifier(output, texts) == 4 && calls.count == 1 &&
          calls.calls[0][0] == 1 && identifier(cache.group, groups) == 2);
    calls.count = 0;
    calls.fail = true;
    output = span(texts[2]);
    CHECK(!ab_ui_localization_string(&source, span(texts[1]), &cache, &output, NULL) &&
          identifier(output, texts) == 2 && cache.present &&
          identifier(cache.group, groups) == 2);
    CHECK(!ab_ui_localization_string(NULL, span(texts[1]), &cache, &output, NULL));
    puts(
        "UI localization order, cache skip/update, repeated BASIC and profile lookups, "
        "empty results/identifiers and failure-output checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_LOCALIZATION_V1 32 256\n") == 0);
    size_t total_calls = 0, empty_results = 0, cached_commands = 0;
    for (size_t configuration = 0; configuration < 32; ++configuration) {
        uint32_t setup[4], group_ids[6];
        CHECK(words(file, setup, 4) && setup[0] < 2 && setup[1] <= 6 && setup[2] < 2 &&
              setup[3] < 5 && (setup[0] || !setup[1]));
        AbSpan profile[6];
        if (setup[1]) {
            CHECK(words(file, group_ids, setup[1]));
            for (uint32_t index = 0; index < setup[1]; ++index) {
                CHECK(group_ids[index] < 5);
                profile[index] = span(groups[group_ids[index]]);
            }
        }
        AbUiLocalizationCache cache = {span(groups[setup[3]]), setup[2] != 0};
        Boundaries calls = {.cache = &cache};
        for (size_t group = 0; group < 5; ++group) {
            CHECK(words(file, calls.responses[group], 5));
            for (size_t id = 0; id < 5; ++id) {
                CHECK(calls.responses[group][id] < 5);
            }
        }
        AbUiLocalizationSource source = {profile, setup[1], &calls, lookup};
        for (size_t step = 0; step < 256; ++step) {
            uint32_t command[5];
            CHECK(words(file, command, 5) && command[0] < 5 && command[1] < 2 &&
                  command[2] < 2 && command[3] < 2 && command[4] < 5);
            if (command[2]) {
                cache.present = command[3] != 0;
                cache.group = span(groups[command[4]]);
            }
            calls.count = 0;
            AbSpan output;
            CHECK(ab_ui_localization_string(&source, span(texts[command[0]]),
                                            command[1] ? &cache : NULL, &output, NULL));
            uint32_t actual[] = {identifier(output, texts), cache.present,
                                 cache.present ? identifier(cache.group, groups) : 0,
                                 (uint32_t)calls.count};
            uint32_t expected[4];
            CHECK(words(file, expected, 4) && expected[3] <= 8);
            if (memcmp(actual, expected, sizeof(actual)) != 0) {
                fprintf(stderr, "UI localization case %zu/%zu differs\n", configuration,
                        step);
                return false;
            }
            for (size_t index = 0; index < calls.count; ++index) {
                uint32_t call[5];
                CHECK(words(file, call, 5));
                if (memcmp(call, calls.calls[index], sizeof(call)) != 0) {
                    fprintf(stderr, "UI localization %zu/%zu callback %zu differs\n",
                            configuration, step, index);
                    return false;
                }
            }
            total_calls += calls.count;
            empty_results += !output.size;
            cached_commands += command[1] != 0;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI localization reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "All 8192 original localization commands, %zu group lookups, %zu empty results "
        "and %zu commands with cache match.\n",
        total_calls, empty_results, cached_commands);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
