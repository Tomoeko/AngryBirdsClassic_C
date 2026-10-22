#include "ab/museum_glue.h"
#include "museum_glue_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Museum glue check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbMuseumGlueObject objects[3];
    uint32_t input[6], events[3][4], count, fail_at;
    size_t words;
} GlueHost;

static const char *const names[] = {"first",   "second",          "plunger",
                                    "renamed", "renamed_plunger", "prior"};

static AbSpan name_span(size_t index) {
    return (AbSpan){(const uint8_t *)names[index], strlen(names[index])};
}

static uint32_t name_id(AbSpan name) {
    for (uint32_t index = 0; index < 6; ++index) {
        AbSpan candidate = name_span(index);
        if (name.size == candidate.size && name.data &&
            memcmp(name.data, candidate.data, name.size) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue scalar(uint32_t kind) {
    static AbDataTable empty = {0};
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN};
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        case 3:
            return number(0);
        case 4:
            return (AbDataValue){.kind = AB_DATA_STRING, .string = name_span(5)};
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static bool event(GlueHost *host, uint32_t operation, AbSpan name, AbError *error) {
    CHECK(host->count < 3 && name_id(name));
    uint32_t *values = host->events[host->count++];
    values[0] = operation;
    values[1] = name_id(name);
    values[2] = values[3] = 0;
    return host->fail_at != operation || ab_fail(error, "Controlled glue host failure");
}

static bool collider(void *context, AbSpan name, AbDataValue *output, AbError *error) {
    GlueHost *host = context;
    if (!event(host, 1, name, error)) {
        return false;
    }
    *output = number((float)host->input[2]);
    return true;
}

static size_t plunger_index(const GlueHost *host) {
    static const size_t indices[] = {2, 0, 1};
    return indices[host->input[3]];
}

static bool object(void *context, AbSpan name, AbMuseumGlueObject **output,
                   AbError *error) {
    GlueHost *host = context;
    if (!event(host, 2, name, error)) {
        return false;
    }
    *output = host->objects + plunger_index(host);
    return true;
}

static bool velocity(void *context, AbSpan name, AbVec2 value, AbError *error) {
    GlueHost *host = context;
    uint32_t bits[2];
    memcpy(bits, &value, sizeof(bits));
    CHECK(bits[0] == 0 && bits[1] == 0);
    if (!event(host, 3, name, error)) {
        return false;
    }
    if (host->input[5] & 1u) {
        host->objects[1].name = name_span(3);
    }
    if (host->input[5] & 2u) {
        host->objects[plunger_index(host)].name = name_span(4);
    }
    return true;
}

static void prepare(GlueHost *host, uint32_t sample) {
    *host = (GlueHost){0};
    museum_glue_case(sample, host->input);
    for (uint32_t index = 0; index < 3; ++index) {
        host->objects[index] = (AbMuseumGlueObject){
            .name = name_span(index),
            .grabber = scalar(index == 1 ? host->input[1] : 0),
            .grabbed_egg = scalar(host->input[4]),
            .glued_to_block = scalar((host->input[4] + index) % 4u)};
    }
    host->objects[0].plunger_name = name_span(plunger_index(host));
}

static AbMuseumGlueHost glue_host(GlueHost *host) {
    return (AbMuseumGlueHost){host, collider, object, velocity};
}

static void value_words(AbDataValue value, uint32_t output[2]) {
    output[1] = 0;
    switch (value.kind) {
        case AB_DATA_NIL:
            output[0] = 0;
            break;
        case AB_DATA_BOOLEAN:
            output[0] = 1;
            output[1] = value.bits != 0;
            break;
        case AB_DATA_NUMBER:
            output[0] = 3;
            output[1] = value.bits;
            break;
        case AB_DATA_STRING:
            output[0] = 4;
            output[1] = name_id(value.string);
            break;
        default:
            output[0] = 5;
            break;
    }
}

static bool compare(FILE *file, GlueHost *host, const char *section,
                    const uint32_t *actual, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[field]) {
            fprintf(stderr,
                    "Glue case %" PRIu32 " %s[%zu]: C %08" PRIx32 ", native %08" PRIx32
                    "\n",
                    host->input[0], section, field, actual[field], expected);
            return false;
        }
    }
    CHECK(fgetc(file) == '\n');
    host->words += count;
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MUSEUM_GLUE_V1 4096\n") == 0);
    size_t words = 0, pairs = 0, events = 0, eggs = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        GlueHost host;
        prepare(&host, sample);
        CHECK(compare(file, &host, "input", host.input, 6));
        AbMuseumGlueResult result;
        CHECK(ab_museum_glue_contact(host.objects, host.objects + 1, glue_host(&host),
                                     &result, NULL));
        uint32_t response[] = {1, result.disable_contact, result.has_pair ? 5u : 0u,
                               result.has_pair ? name_id(result.first) : 0,
                               result.has_pair ? name_id(result.second) : 0};
        CHECK(compare(file, &host, "response", response, 5));
        pairs += result.has_pair;
        eggs += result.has_pair && host.input[2] == 6;
        for (size_t index = 0; index < 3; ++index) {
            const AbMuseumGlueObject *entry = host.objects + index;
            uint32_t fields[7] = {name_id(entry->name)};
            value_words(entry->grabber, fields + 1);
            value_words(entry->grabbed_egg, fields + 3);
            value_words(entry->glued_to_block, fields + 5);
            CHECK(compare(file, &host, "object", fields, 7));
        }
        CHECK(compare(file, &host, "count", &host.count, 1));
        for (size_t index = 0; index < host.count; ++index) {
            CHECK(compare(file, &host, "event", host.events[index], 4));
        }
        words += host.words;
        events += host.count;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB museum glue reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4,096 native glue helpers match: %zu words, %zu pairs, %zu egg "
           "grabs and %zu ordered host calls.\n",
           words, pairs, eggs, events);
    return true;
}

static bool edge_checks(void) {
    GlueHost host;
    prepare(&host, 36); /* Unclaimed egg, separate plunger. */
    CHECK(host.input[1] == 0 && host.input[2] == 6 && plunger_index(&host) == 2);
    AbMuseumGlueResult result = {.disable_contact = true, .first = name_span(5)};
    AbMuseumGlueResult before = result;
    CHECK(!ab_museum_glue_contact(NULL, host.objects + 1, glue_host(&host), &result,
                                  NULL) &&
          memcmp(&result, &before, sizeof(result)) == 0);
    for (uint32_t operation = 1; operation <= 3; ++operation) {
        prepare(&host, 36);
        host.fail_at = operation;
        CHECK(!ab_museum_glue_contact(host.objects, host.objects + 1, glue_host(&host),
                                      &result, NULL) &&
              memcmp(&result, &before, sizeof(result)) == 0 &&
              host.count == operation &&
              host.objects[1].glued_to_block.kind == AB_DATA_BOOLEAN &&
              !host.objects[1].glued_to_block.bits);
        if (operation == 3) {
            CHECK(host.objects[1].grabber.kind == AB_DATA_STRING &&
                  name_id(host.objects[1].grabber.string) == 3 &&
                  host.objects[2].grabbed_egg.kind == AB_DATA_STRING &&
                  name_id(host.objects[2].grabbed_egg.string) == 2);
        } else {
            CHECK(host.objects[1].grabber.kind == AB_DATA_NIL);
        }
    }
    prepare(&host, 2);
    host.objects[1].name = (AbSpan){NULL, 1};
    CHECK(ab_museum_glue_contact(host.objects, host.objects + 1, (AbMuseumGlueHost){0},
                                 &result, NULL) &&
          !result.has_pair && !result.disable_contact);
    return true;
}

#include "museum_glue_route.h"

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    bool okay = edge_checks() && route_edge_checks();
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2) {
        char header[100];
        okay = okay && file && fgets(header, sizeof(header), file) &&
               fseek(file, 0, SEEK_SET) == 0;
        if (okay) {
            okay = strcmp(header, "AB_ANDROID_MUSEUM_GLUE_ROUTE_V1 8192\n") == 0
                       ? native_route_checks(file)
                       : native_checks(file);
        }
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
