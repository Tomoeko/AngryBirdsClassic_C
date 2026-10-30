#include "ab/block_removal_cursor.h"
#include "block_removal_cursor_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal cursor failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbStringMap *maps[2];
    AbDataTable tables[9];
    AbDataEntry raw[6][4], world[6], container;
    AbDataValue values[2][6];
    AbBlockRemovalState state;
    AbBlockRemovalCursor cursor;
    AbBlockRemovalHost host;
    uint32_t input[16], events[6][8], count;
    bool fail_host;
} Fixture;

static const char *const names[] = {"Amber", "Pine",   "Bruno",
                                    "Lucid", "Chrome", "Zinc"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue table(Fixture *fixture, size_t index) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + index};
}

static AbDataValue scalar(uint32_t kind) {
    if (kind == 0) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    return kind == 3 ? (AbDataValue){.kind = AB_DATA_NUMBER} : text("truthy");
}

static AbDataValue strength(uint32_t kind, uint32_t flags) {
    if (kind <= 2) {
        return scalar(kind);
    }
    uint32_t word =
        kind == 3   ? (flags & 1u ? UINT32_C(0x80000000) : 0u)
        : kind == 4 ? (flags & 2u ? UINT32_C(0xff800000) : UINT32_C(0xbf800000))
        : kind == 5 ? (flags & 4u ? UINT32_C(0x7f800000) : UINT32_C(0x3f800000))
                    : UINT32_C(0x7fc01234);
    return kind == 7 ? text("truthy")
                     : (AbDataValue){.kind = AB_DATA_NUMBER, .bits = word};
}

static uint32_t name_id(AbSpan name) {
    for (uint32_t index = 0; index < 6; ++index) {
        if (name.size == strlen(names[index]) &&
            !memcmp(name.data, names[index], name.size)) {
            return index + 1u;
        }
    }
    return 0;
}

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        output[1] = name_id(value.string);
        if (!output[1]) {
            output[1] = 7;
        }
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    }
}

static uint32_t current_map(Fixture *fixture) {
    return !fixture->state.dead_blocks                      ? 0u
           : fixture->state.dead_blocks == fixture->maps[0] ? 1u
                                                            : 2u;
}

static bool mask(AbStringMap *map, uint32_t *output) {
    *output = 0;
    for (uint32_t index = 0; index < 6; ++index) {
        uintptr_t value = 0;
        bool present = false;
        CHECK(ab_string_map_get(map, span(names[index]), &value, &present, NULL));
        if (present) {
            *output |= 1u << index;
        }
    }
    return true;
}

static bool snapshot(Fixture *fixture, uint32_t output[77]) {
    value_words(fixture, fixture->state.objects, output);
    value_words(fixture, fixture->container.value, output + 2);
    output[4] = current_map(fixture);
    for (size_t map = 0; map < 2; ++map) {
        for (size_t index = 0; index < 6; ++index) {
            uintptr_t handle = 0;
            bool present = false;
            CHECK(ab_string_map_get(fixture->maps[map], span(names[index]), &handle,
                                    &present, NULL));
            AbDataValue value =
                present ? *(const AbDataValue *)handle : (AbDataValue){0};
            value_words(fixture, value, output + 5 + map * 12u + index * 2u);
        }
    }
    for (size_t index = 0; index < 6; ++index) {
        for (size_t field = 0; field < 4; ++field) {
            value_words(fixture, fixture->raw[index][field].value,
                        output + 29 + index * 8u + field * 2u);
        }
    }
    return true;
}

static bool rocket(void *context, AbDataValue name, AbError *error) {
    Fixture *fixture = context;
    CHECK(fixture->count < 6);
    uint32_t *event = fixture->events[fixture->count];
    value_words(fixture, name, event);
    event[2] = current_map(fixture);
    CHECK(mask(fixture->maps[0], event + 3) && mask(fixture->maps[1], event + 4));
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    event[5] = types[fixture->state.objects.kind];
    event[6] = types[fixture->container.value.kind];
    event[7] = fixture->count++;
    uint32_t mutation = fixture->input[7];
    if (mutation & 1u) {
        fixture->state.dead_blocks = fixture->maps[1];
    }
    if (mutation & 2u) {
        fixture->container.value = (AbDataValue){0};
    }
    if (mutation & 4u) {
        fixture->state.dead_blocks = NULL;
    }
    if (mutation & 8u) {
        for (size_t index = 0; index < 6; ++index) {
            fixture->world[index].value = scalar(1);
        }
    }
    if (mutation & 16u) {
        fixture->raw[5][0].value = (AbDataValue){0};
    }
    if (mutation & 32u) {
        fixture->state.objects = scalar(1);
    }
    if (mutation & 64u) {
        uintptr_t value = 0;
        bool present = false;
        CHECK(ab_string_map_get(fixture->maps[0], span(names[0]), &value, &present,
                                error));
        if (present) {
            fixture->values[0][0] = scalar(1);
        }
    }
    if (mutation & 128u) {
        fixture->state.dead_blocks = fixture->state.dead_blocks == fixture->maps[0]
                                         ? fixture->maps[1]
                                         : fixture->maps[0];
    }
    return fixture->fail_host ? ab_fail(error, "Injected rocket host failure") : true;
}

static bool prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    block_removal_cursor_case(sample, fixture->input);
    const char *const fields[] = {"strength", "rocket", "deathTimerFull", "name"};
    for (uint32_t index = 0; index < 6; ++index) {
        uint32_t k = (fixture->input[10] >> (index * 3u)) & 7u;
        fixture->raw[index][0] =
            (AbDataEntry){text(fields[0]), strength(k, fixture->input[15])};
        fixture->raw[index][1] = (AbDataEntry){
            text(fields[1]), scalar((fixture->input[11] >> (index * 2u)) & 3u)};
        fixture->raw[index][2] = (AbDataEntry){
            text(fields[2]), scalar((fixture->input[12] >> (index * 2u)) & 3u)};
        k = (fixture->input[13] >> (index * 2u)) & 3u;
        fixture->raw[index][3] =
            (AbDataEntry){text(fields[3]), k == 0   ? text(names[index])
                                           : k == 1 ? scalar(0)
                                           : k == 2 ? scalar(1)
                                                    : scalar(3)};
        fixture->tables[index] = (AbDataTable){fixture->raw[index], 4, 4, NULL};
        k = (fixture->input[5] >> (index * 2u)) & 3u;
        fixture->world[index] =
            (AbDataEntry){text(names[index]), k == 3   ? table(fixture, index)
                                              : k == 2 ? scalar(3)
                                                       : scalar(k)};
        k = (fixture->input[9] >> (index * 2u)) & 3u;
        fixture->values[0][index] = fixture->values[1][index] =
            k == 3   ? table(fixture, index)
            : k == 2 ? scalar(3)
                     : scalar(k + 1u);
    }
    fixture->tables[6] = (AbDataTable){fixture->world, 6, 6, NULL};
    uint32_t world = fixture->input[2];
    fixture->container = (AbDataEntry){text("world"), world == 6   ? table(fixture, 6)
                                                      : world == 5 ? table(fixture, 8)
                                                                   : scalar(world)};
    fixture->tables[7] = (AbDataTable){&fixture->container, 1, 1, NULL};
    for (size_t map = 0; map < 2; ++map) {
        CHECK(ab_string_map_open(fixture->input[map ? 14 : 6], 16, 16384,
                                 fixture->maps + map, NULL));
        uint32_t live = fixture->input[map ? 8 : 4];
        for (uint32_t index = 0; index < 6; ++index) {
            if (live & (1u << index)) {
                CHECK(ab_string_map_set(fixture->maps[map], span(names[index]),
                                        (uintptr_t)&fixture->values[map][index], true,
                                        NULL));
            }
        }
    }
    uint32_t objects = fixture->input[1];
    fixture->state = (AbBlockRemovalState){objects == 6   ? table(fixture, 7)
                                           : objects == 5 ? table(fixture, 8)
                                                          : scalar(objects),
                                           fixture->input[3] ? fixture->maps[0] : NULL};
    fixture->host = (AbBlockRemovalHost){fixture, rocket};
    return true;
}

static void close_fixture(Fixture *fixture) {
    ab_string_map_close(fixture->maps[0]);
    ab_string_map_close(fixture->maps[1]);
}

static bool invoke(Fixture *fixture, AbBlockRemovalEntry *entry, bool *found,
                   AbError *error) {
    return ab_block_removal_open(&fixture->state, &fixture->cursor, error) &&
           ab_block_removal_next(&fixture->state, &fixture->cursor, &fixture->host,
                                 entry, found, error);
}

static bool edges(void) {
    Fixture fixture;
    CHECK(prepare(&fixture, 0));
    AbBlockRemovalEntry entry = {.key = span("preserved")};
    bool found = true;
    CHECK(invoke(&fixture, &entry, &found, NULL) && !found && fixture.cursor.finished);
    CHECK(name_id(entry.key) == 0 && entry.key.size == 9);
    close_fixture(&fixture);
    CHECK(prepare(&fixture, 8));
    fixture.fail_host = true;
    CHECK(!invoke(&fixture, &entry, &found, NULL) && fixture.count == 1);
    uint32_t live = 0;
    CHECK(mask(fixture.maps[0], &live) && live == 63);
    close_fixture(&fixture);
    CHECK(prepare(&fixture, 13));
    CHECK(invoke(&fixture, &entry, &found, NULL) && found);
    CHECK(ab_string_map_set(fixture.state.dead_blocks, entry.key, 0, false, NULL));
    CHECK(ab_block_removal_next(&fixture.state, &fixture.cursor, &fixture.host, &entry,
                                &found, NULL) &&
          found);
    close_fixture(&fixture);
    AbStringMap *map = NULL;
    CHECK(ab_string_map_open(0, 8, 16384, &map, NULL));
    AbDataEntry strength = {text("strength"),
                            {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)}};
    AbDataTable physical = {&strength, 1, 1, NULL};
    AbDataValue value = {.kind = AB_DATA_TABLE, .table = &physical};
    const char *const pending[] = {"a", "b", "c", "d", "e", "f", "g", "h", "o"};
    AbDataEntry world_entries[9];
    for (size_t index = 0; index < 9; ++index) {
        world_entries[index] = (AbDataEntry){text(pending[index]), value};
        if (index < 8)
            CHECK(ab_string_map_set(map, span(pending[index]), (uintptr_t)&value, true,
                                    NULL));
    }
    for (size_t index = 0; index < 8; ++index)
        if (index != 6)
            CHECK(ab_string_map_set(map, span(pending[index]), 0, false, NULL));
    AbDataTable world = {world_entries, 9, 9, NULL};
    AbDataEntry world_field = {text("world"), {.kind = AB_DATA_TABLE, .table = &world}};
    AbDataTable objects = {&world_field, 1, 1, NULL};
    AbBlockRemovalState state = {{.kind = AB_DATA_TABLE, .table = &objects}, map};
    AbBlockRemovalCursor cursor;
    CHECK(ab_block_removal_open(&state, &cursor, NULL) &&
          ab_block_removal_next(&state, &cursor, NULL, &entry, &found, NULL) && found &&
          entry.key.size == 1 && entry.key.data[0] == 'g' && cursor.cursor == 7);
    /* An explosion inserts a new death while the current key is still live.
     * Eight retained constructor keys force a rehash from eight slots to two. */
    CHECK(ab_string_map_set(map, span("o"), (uintptr_t)&value, true, NULL));
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(map, &layout) && layout.capacity == 2 &&
          layout.count == 2 && ab_string_map_set(map, span("g"), 0, false, NULL) &&
          ab_block_removal_next(&state, &cursor, NULL, &entry, &found, NULL) && found &&
          entry.key.size == 1 && entry.key.data[0] == 'o' && cursor.cursor == 2);
    AbBlockRemovalCursor end = cursor;
    CHECK(ab_block_removal_next(&state, &end, NULL, &entry, &found, NULL) && !found &&
          end.finished);
    /* Native next rejects when mutation actually discards its retained key;
     * do not hide that error by restarting or reporting an empty queue. */
    CHECK(ab_string_map_set(map, span("o"), 0, false, NULL) &&
          ab_string_map_set(map, span("b"), (uintptr_t)&value, true, NULL) &&
          !ab_block_removal_next(&state, &cursor, NULL, &entry, &found, NULL));
    ab_string_map_close(map);
    puts("Removal traversal resumes its retained key after explosion-driven hash "
         "shrink, visits the pending death and rejects a discarded iterator key.");
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || actual[field] != expected) {
            fprintf(stderr, "Removal cursor sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_CURSOR_V1 4096\n"));
    size_t matched = 0, failures = 0, selected = 0, rockets = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        CHECK(prepare(&fixture, sample));
        CHECK(words(file, fixture.input, 16, sample, "input", &matched));
        AbBlockRemovalEntry entry = {0};
        bool found = false;
        uint32_t result[5] = {0};
        result[0] = !invoke(&fixture, &entry, &found, NULL);
        result[1] = !result[0] && found;
        if (result[1]) {
            result[2] = name_id(entry.key);
            value_words(&fixture, entry.object, result + 3);
        }
        CHECK(words(file, result, 5, sample, "result", &matched));
        uint32_t state[77];
        CHECK(snapshot(&fixture, state) &&
              words(file, state, 77, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 8, sample, "event", &matched));
        }
        failures += result[0];
        selected += result[1];
        rockets += fixture.count;
        close_fixture(&fixture);
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB block removal cursor reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal traversal prefixes match: %zu words, %zu "
           "failures, %zu selections and %zu rockets.\n",
           matched, failures, selected, rockets);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
