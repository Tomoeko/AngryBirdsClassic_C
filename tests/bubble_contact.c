#include "ab/bubble_contact.h"
#include "bubble_contact_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bubble contact failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBubbleContactObject objects[2];
    AbRigidTransform transforms[2];
    uint32_t input[11], events[2][12], count, fail_at, geometry_calls;
    size_t words;
} BubbleHost;

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

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
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
            return (AbDataValue){.kind = AB_DATA_STRING, .string = text("seed")};
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static void prepare(BubbleHost *host, uint32_t sample) {
    *host = (BubbleHost){0};
    bubble_contact_case(sample, host->input);
    host->transforms[0] = (AbRigidTransform){.cosine = 1};
    host->transforms[1] = (AbRigidTransform){{1.5f, -0.125f}, 0, 1};
    for (size_t object = 0; object < 2; ++object) {
        AbDataValue fields[7];
        for (size_t field = 0; field < 7; ++field) {
            fields[field] = scalar(object == 0 && field < 2
                                       ? host->input[1 + field]
                                       : (host->input[9] + (uint32_t)field) % 6u);
        }
        host->objects[object] =
            (AbBubbleContactObject){.name = text(object ? "bubble" : "bird"),
                                    .in_bubble = fields[0],
                                    .shot = fields[1],
                                    .strength = fields[2],
                                    .kill_silently = fields[3],
                                    .anti_gravity_timer = fields[4],
                                    .sprite = fields[5],
                                    .sprite_scale = fields[6]};
    }
    host->objects[0].body_velocity =
        (AbVec2){number(host->input[4]), number(host->input[5])};
    host->objects[0].pending = (AbDeferredVelocity){
        {number(host->input[7]), number(host->input[8])}, host->input[6] != 0};
}

static bool record_event(BubbleHost *host, uint32_t kind, uint32_t **output) {
    CHECK(host->count < 2);
    const AbDeferredVelocity *pending = &host->objects[0].pending;
    uint32_t *event = host->events[host->count++];
    event[0] = kind;
    event[1] = 7;
    event[2] = pending->pending;
    event[3] = bits(pending->velocity.x);
    event[4] = bits(pending->velocity.y);
    event[5] = 1;
    *output = event;
    return true;
}

static bool trapped(void *context, AbBubbleContactObject *object, AbError *error) {
    BubbleHost *host = context;
    uint32_t *event;
    CHECK(object == host->objects && host->objects[1].dead &&
          host->objects[1].strength.kind == AB_DATA_NUMBER &&
          !host->objects[1].strength.bits && host->objects[1].kill_silently.bits &&
          object->in_bubble.kind == AB_DATA_BOOLEAN && object->in_bubble.bits &&
          object->anti_gravity_timer.kind == AB_DATA_NUMBER &&
          object->anti_gravity_timer.bits == UINT32_C(0x40a00000) &&
          object->sprite.kind == AB_DATA_STRING && object->sprite.string.size == 17 &&
          memcmp(object->sprite.string.data, "BUBBLE_PARTICLE_6", 17) == 0 &&
          object->sprite_scale.kind == AB_DATA_NUMBER &&
          object->sprite_scale.bits == UINT32_C(0x3f000000) &&
          record_event(host, 1, &event));
    if (host->fail_at == 1) {
        return ab_fail(error, "Controlled trapped callback failure");
    }
    if (host->input[10] & 1u) {
        host->transforms[0].position.x = 0.25f;
    }
    if (host->input[10] & 2u) {
        object->shot = scalar(1);
    }
    if (host->input[10] & 4u) {
        object->pending = (AbDeferredVelocity){{7, 8}, false};
    }
    return true;
}

static bool geometry(void *context, AbWorldManifold *output, AbError *error) {
    BubbleHost *host = context;
    ++host->geometry_calls;
    if (host->fail_at == 2) {
        return ab_fail(error, "Controlled bubble geometry failure");
    }
    if (host->fail_at == 4) {
        output->normal.x = NAN;
        return true;
    }
    AbManifold manifold = {.kind = AB_CONTACT_CIRCLES, .point_count = 1};
    ab_world_manifold(output, &manifold, host->transforms[host->input[3]], 1,
                      host->transforms[1u - host->input[3]], 1);
    return true;
}

static bool collision(void *context, AbBubbleCollisionCall *call, AbError *error) {
    BubbleHost *host = context;
    uint32_t *event;
    CHECK(call->first.size == 4 && memcmp(call->first.data, "bird", 4) == 0 &&
          call->second.size == 6 && memcmp(call->second.data, "bubble", 6) == 0 &&
          !call->response.count && !call->response.entries &&
          record_event(host, 2, &event));
    event[6] = 2;
    event[7] = bits(call->normal.x);
    event[8] = bits(call->normal.y);
    event[9] = bits(call->point.x);
    event[10] = bits(call->point.y);
    event[11] = 1;
    if (host->fail_at == 3) {
        return ab_fail(error, "Controlled bubble collision failure");
    }
    static AbDataEntry ignored[3];
    ignored[0] = (AbDataEntry){
        {.kind = AB_DATA_STRING, .string = text("shouldDisableContact")}, scalar(2)};
    ignored[1] = (AbDataEntry){
        {.kind = AB_DATA_STRING, .string = text("newCollisionPathParameters")},
        scalar(5)};
    ignored[2] =
        (AbDataEntry){{.kind = AB_DATA_STRING, .string = text("glueContactPair")},
                      {.kind = AB_DATA_STRING, .string = text("ignored")}};
    call->response = (AbDataTable){ignored, 3, 3, NULL};
    return true;
}

static AbBubbleContactHost callbacks(BubbleHost *host) {
    return (AbBubbleContactHost){host, trapped, geometry, collision};
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
            CHECK(value.string.data);
            if (value.string.size == 4 && memcmp(value.string.data, "seed", 4) == 0) {
                output[1] = 1;
            } else {
                CHECK(value.string.size == 17 &&
                      memcmp(value.string.data, "BUBBLE_PARTICLE_6", 17) == 0);
                output[1] = 2;
            }
            break;
        default:
            output[0] = 5;
            break;
    }
    return true;
}

static bool compare(FILE *file, BubbleHost *host, const char *section,
                    const uint32_t *actual, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[field]) {
            fprintf(stderr,
                    "Bubble case %" PRIu32 " %s[%zu]: C %08" PRIx32
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
          strcmp(line, "AB_ANDROID_BUBBLE_CONTACT_V1 4096\n") == 0);
    size_t words = 0, captures = 0, collisions = 0, dead = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        BubbleHost host;
        prepare(&host, sample);
        CHECK(compare(file, &host, "input", host.input, 11) &&
              ab_bubble_contact_begin(host.objects, host.objects + 1, callbacks(&host),
                                      NULL));
        const AbDeferredVelocity *pending = &host.objects[0].pending;
        uint32_t summary[] = {sample,
                              host.count,
                              7,
                              pending->pending,
                              bits(pending->velocity.x),
                              bits(pending->velocity.y),
                              bits(host.transforms[0].position.x),
                              bits(host.transforms[0].position.y),
                              host.objects[1].dead ? 5u : 0u,
                              host.objects[1].dead};
        CHECK(compare(file, &host, "summary", summary, 10));
        dead += host.objects[1].dead;
        for (size_t object = 0; object < 2; ++object) {
            const AbBubbleContactObject *entry = host.objects + object;
            AbDataValue values[] = {entry->in_bubble,          entry->shot,
                                    entry->strength,           entry->kill_silently,
                                    entry->anti_gravity_timer, entry->sprite,
                                    entry->sprite_scale};
            uint32_t fields[14];
            for (size_t field = 0; field < 7; ++field) {
                CHECK(value_words(values[field], fields + field * 2));
            }
            CHECK(compare(file, &host, object ? "bubble" : "collider", fields, 14));
        }
        for (size_t event = 0; event < host.count; ++event) {
            CHECK(compare(file, &host, "event", host.events[event], 12));
            captures += host.events[event][0] == 1;
            collisions += host.events[event][0] == 2;
        }
        words += host.words;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB bubble contact reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4,096 native bubble contacts match: %zu words, %zu new captures, "
           "%zu collision callbacks and %zu dead bubbles.\n",
           words, captures, collisions, dead);
    return true;
}

static bool edge_checks(void) {
    BubbleHost host;
    for (uint32_t failure = 1; failure <= 4; ++failure) {
        prepare(&host, 12); /* New capture with Boolean-true shot. */
        host.fail_at = failure;
        AbError error = {0};
        CHECK(!ab_bubble_contact_begin(host.objects, host.objects + 1, callbacks(&host),
                                       &error) &&
              error.message[0] && host.objects[1].dead &&
              host.objects[0].pending.pending &&
              host.objects[0].in_bubble.kind == AB_DATA_BOOLEAN &&
              host.objects[0].in_bubble.bits &&
              host.count == (failure == 3 ? 2u : 1u) &&
              host.geometry_calls == (failure > 1));
    }
    prepare(&host, 0);
    CHECK(ab_bubble_contact_begin(host.objects, host.objects + 1,
                                  (AbBubbleContactHost){0}, NULL) &&
          host.objects[1].dead && !host.count && !host.geometry_calls &&
          !ab_bubble_contact_begin(host.objects, host.objects, callbacks(&host), NULL));
    prepare(&host, 2);
    host.objects[1].dead = true;
    AbBubbleContactObject before = host.objects[1];
    CHECK(ab_bubble_contact_begin(host.objects, host.objects + 1,
                                  (AbBubbleContactHost){0}, NULL) &&
          memcmp(&before, host.objects + 1, sizeof(before)) == 0);
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
