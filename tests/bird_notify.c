#include "ab/bird_collision_notify.h"
#include "bird_notify_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird notification failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct NotifyHost NotifyHost;
typedef struct {
    NotifyHost *host;
    size_t index;
} ScriptContext;

struct NotifyHost {
    AbBirdCollisionNotifyHost callbacks;
    AbBirdCollisionObject objects[2];
    AbBlockCollisionDefinition definitions[2];
    AbBirdCollisionScript scripts[2];
    ScriptContext contexts[2];
    uint32_t input[16], events[16][14], count;
    size_t words;
};

static const char *const names[] = {"first", "second"};
static const char *const values[] = {"truthy", "first_handler", "second_handler"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint32_t string_id(AbSpan name, const char *const *strings, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (name.size == strlen(strings[index]) && name.data &&
            memcmp(name.data, strings[index], name.size) == 0) {
            return (uint32_t)index + 1;
        }
    }
    return 0;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
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
            return (AbDataValue){.kind = AB_DATA_NUMBER};
        case 4:
            return text("truthy");
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static uint32_t object_id(const NotifyHost *host,
                          const AbBlockCollisionObject *object) {
    for (uint32_t id = 1; id <= 2; ++id) {
        if (object == &host->objects[id - 1u].object) {
            return id;
        }
    }
    return 0;
}

static uint32_t *record(NotifyHost *host, uint32_t kind, uint32_t first,
                        uint32_t second, uint32_t receiver) {
    if (host->count >= 16) {
        return NULL;
    }
    uint32_t *event = host->events[host->count++];
    event[0] = kind;
    event[1] = first;
    event[2] = second;
    event[3] = receiver;
    event[12] = truthy(host->definitions[1].on_collision);
    for (size_t index = 0; index < 2; ++index) {
        if (truthy(host->objects[index].object.sensor)) {
            event[13] |= 1u << index;
        }
    }
    if (truthy(host->objects[1].controllable)) {
        event[13] |= 4u;
    }
    if (host->objects[1].script) {
        event[13] |= 8u;
    }
    return event;
}

static bool notice_event(NotifyHost *host, uint32_t kind, uint32_t receiver,
                         const AbBlockCollisionNotice *notice) {
    uint32_t first = object_id(host, notice->first),
             second = object_id(host, notice->second);
    CHECK(first && second);
    uint32_t *event = record(host, kind, first, second, receiver);
    CHECK(event);
    event[4] = bits(notice->force);
    event[5] = bits(notice->damage);
    if (notice->normal_present) {
        event[6] = bits(notice->normal.x);
        event[7] = bits(notice->normal.y);
        event[11] = 3;
    }
    if (notice->point_present) {
        event[8] = bits(notice->point.x);
        event[9] = bits(notice->point.y);
        event[10] = 3;
    }
    return true;
}

static bool object(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    (void)error;
    NotifyHost *host = context;
    uint32_t id = string_id(name, names, 2);
    CHECK(id);
    *output = host->objects + id - 1u;
    return true;
}

static bool level(void *context, const AbBlockCollisionNotice *notice, AbError *error) {
    (void)error;
    NotifyHost *host = context;
    CHECK(!notice->normal_present && !notice->point_present &&
          notice_event(host, 1, 0, notice));
    if (host->input[7] & 16u) {
        host->objects[1].object.sensor = scalar(1);
    }
    return true;
}

static bool definition(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error) {
    (void)error;
    NotifyHost *host = context;
    uint32_t id = string_id(name, names, 2);
    CHECK(id && record(host, 2, id, 0, 0));
    *output = host->definitions + id - 1u;
    return true;
}

static bool named(void *context, AbDataValue name, const AbBlockCollisionNotice *notice,
                  AbError *error) {
    NotifyHost *host = context;
    CHECK(name.kind == AB_DATA_STRING);
    uint32_t id = string_id(name.string, values, 3);
    CHECK(id == 2 || id == 3);
    if (!(host->input[6] & (32u << (id - 2u)))) {
        return ab_fail(error, "Required named bird collision function is absent");
    }
    CHECK(notice->normal_present && !notice->point_present &&
          notice_event(host, id == 2 ? 3u : 4u, 0, notice));
    if (id == 2) {
        if (host->input[7] & 1u) {
            host->definitions[1].on_collision = scalar(0);
        }
        if (host->input[7] & 8u) {
            host->objects[1].controllable = scalar(2);
        }
    }
    return true;
}

static bool scripts(void *context, const AbBlockCollisionNotice *notice,
                    AbError *error) {
    (void)error;
    CHECK(notice->normal_present && !notice->point_present);
    return notice_event(context, 5, 0, notice);
}

static bool game_method(void *context, const AbBlockCollisionNotice *notice,
                        AbError *error) {
    (void)error;
    ScriptContext *script = context;
    NotifyHost *host = script->host;
    CHECK(notice->normal_present &&
          notice_event(host, 6, (uint32_t)script->index + 1, notice));
    if (!script->index) {
        uint32_t changes = host->input[7];
        if (changes & 6u) {
            host->objects[0].object.sensor = scalar(changes & 2u ? 2 : 1);
        }
        if (changes & 32u) {
            host->objects[1].script = NULL;
        }
    }
    return true;
}

static void prepare(NotifyHost *host, uint32_t sample) {
    *host = (NotifyHost){0};
    bird_notify_case(sample, host->input);
    host->callbacks =
        (AbBirdCollisionNotifyHost){.context = host,
                                    .object = object,
                                    .level = host->input[6] & 1u ? level : NULL,
                                    .definition = definition,
                                    .named = named,
                                    .scripts = scripts};
    for (size_t index = 0; index < 2; ++index) {
        host->contexts[index] = (ScriptContext){host, index};
        host->scripts[index] =
            (AbBirdCollisionScript){host->contexts + index, game_method};
        host->objects[index] = (AbBirdCollisionObject){
            .object = {.name = text(names[index]).string,
                       .sensor = scalar(host->input[3 + index])},
            .projectile_feather = scalar(host->input[1 + index]),
            .controllable = index ? scalar(host->input[5]) : scalar(0),
            .script = host->input[6] & (8u << index) ? host->scripts + index : NULL};
        host->definitions[index].on_collision =
            host->input[6] & (2u << index) ? text(values[1 + index]) : scalar(0);
    }
}

static AbBirdCollisionNotifyInput input(const NotifyHost *host) {
    return (AbBirdCollisionNotifyInput){
        .contact = {text("first").string,
                    text("second").string,
                    number(host->input[8]),
                    number(host->input[9]),
                    {number(host->input[10]), number(host->input[11])},
                    {number(host->input[12]), number(host->input[13])}},
        .point_present = host->input[14] != 0};
}

static bool value_words(AbDataValue value, uint32_t output[2]) {
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
            output[1] = string_id(value.string, values, 3);
            CHECK(output[1]);
            break;
        case AB_DATA_TABLE:
            output[0] = 5;
            break;
        case AB_DATA_OBJECT:
            return false; /* This native fixture has no C-instance identity map. */
    }
    return true;
}

static bool compare(FILE *file, NotifyHost *host, const char *section,
                    const uint32_t *actual, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[field]) {
            fprintf(stderr,
                    "Bird notify case %" PRIu32 " %s[%zu]: C %08" PRIx32
                    ", native %08" PRIx32 "\n",
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
          strcmp(line, "AB_ANDROID_BIRD_NOTIFY_V1 4096\n") == 0);
    size_t words = 0, calls[7] = {0}, skipped = 0, failures = 0, nil_points = 0,
           reversed = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        NotifyHost host;
        prepare(&host, sample);
        AbBirdCollisionNotifyInput request = input(&host);
        CHECK(compare(file, &host, "input", host.input, 16));
        bool projectile_skipped = false;
        bool okay = ab_bird_collision_notify(&request, &host.callbacks,
                                             &projectile_skipped, NULL);
        uint32_t result[] = {!okay, projectile_skipped};
        CHECK(compare(file, &host, "result", result, 2));
        failures += !okay;
        skipped += projectile_skipped;
        for (size_t object_index = 0; object_index < 2; ++object_index) {
            const AbBirdCollisionObject *entry = host.objects + object_index;
            AbDataValue fields[] = {entry->projectile_feather, entry->object.sensor,
                                    entry->controllable};
            uint32_t output[9];
            for (size_t field = 0; field < 3; ++field) {
                CHECK(value_words(fields[field], output + field * 2));
            }
            output[6] = entry->script != NULL;
            CHECK(
                value_words(host.definitions[object_index].on_collision, output + 7) &&
                compare(file, &host, "object/definition", output, 9));
        }
        CHECK(compare(file, &host, "event count", &host.count, 1));
        for (size_t event = 0; event < host.count; ++event) {
            if (!compare(file, &host, "event", host.events[event], 14)) {
                fprintf(stderr, "Bird notification event %zu kind %" PRIu32 "\n", event,
                        host.events[event][0]);
                return false;
            }
            CHECK(host.events[event][0] < 7);
            ++calls[host.events[event][0]];
            if (host.events[event][0] == 6) {
                nil_points += !host.events[event][10];
                reversed += host.events[event][3] == 2 && host.events[event][1] == 2;
            }
        }
        words += host.words;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB bird notify reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4,096 original bird notification prefixes match: %zu words, %zu "
           "projectile skips, %zu required-binding failures, %zu object methods "
           "(%zu nil-point, %zu reversed second-object calls).\n",
           words, skipped, failures, calls[6], nil_points, reversed);
    for (size_t kind = 1; kind <= 6; ++kind) {
        printf("Notification boundary %zu: %zu calls\n", kind, calls[kind]);
    }
    return true;
}

static bool edge_checks(void) {
    NotifyHost host;
    prepare(&host, 0);
    AbBirdCollisionNotifyInput request = input(&host);
    host.objects[0].projectile_feather = scalar(2);
    request.contact.force = NAN;
    bool skipped = false;
    AbBirdCollisionNotifyHost minimal = {.context = &host, .object = object};
    CHECK(ab_bird_collision_notify(&request, &minimal, &skipped, NULL) && skipped &&
          !host.count);
    prepare(&host, 0);
    request = input(&host);
    host.input[6] &= ~UINT32_C(32);
    skipped = true;
    CHECK(!ab_bird_collision_notify(&request, &host.callbacks, &skipped, NULL) &&
          skipped && host.count == 3 && host.events[2][0] == 2);
    prepare(&host, 0);
    request = input(&host);
    request.point_present = false;
    request.contact.point = (AbVec2){NAN, NAN};
    CHECK(ab_bird_collision_notify(&request, &host.callbacks, &skipped, NULL) &&
          !skipped);
    prepare(&host, 14);
    request = input(&host);
    CHECK(ab_bird_collision_notify(&request, &host.callbacks, &skipped, NULL) &&
          !skipped && host.count);
    for (size_t event = 0; event < host.count; ++event) {
        CHECK(host.events[event][0] != 6);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    bool okay = edge_checks();
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2) {
        okay = okay && file && native_checks(file);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
