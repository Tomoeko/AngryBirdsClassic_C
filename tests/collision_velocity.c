#include "ab/game_contact.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Deferred velocity failed at line %d: %s\n", __LINE__,     \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static float from_bits(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

typedef struct {
    uint32_t flags, proposed[2], geometry[4];
    size_t callbacks, offers;
    AbDeferredVelocity *state;
    AbDataEntry entries[3];
    AbDataTable parameters;
    AbDataValue glue;
    bool fail_callback, fail_lookup;
} ContactHost;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool contact_callback(void *context, AbGameContactCall *call, AbError *error) {
    ContactHost *host = context;
    CHECK(call->first.size == 5 && memcmp(call->first.data, "first", 5) == 0 &&
          call->second.size == 6 && memcmp(call->second.data, "second", 6) == 0 &&
          !call->response.should_disable_contact &&
          call->response.new_collision_parameters.kind == AB_DATA_NIL &&
          call->response.glue_contact_pair.kind == AB_DATA_NIL);
    ++host->callbacks;
    host->geometry[0] = bits(call->normal.x);
    host->geometry[1] = bits(call->normal.y);
    host->geometry[2] = bits(call->point.x);
    host->geometry[3] = bits(call->point.y);
    if (host->fail_callback) {
        return ab_fail(error, "Controlled contact callback failure");
    }
    call->response.should_disable_contact = (host->flags & 4) != 0;
    call->response.glue_contact_pair = host->glue;
    if (host->flags & 2) {
        host->entries[0] = (AbDataEntry){text("birdName"), text("first")};
        host->entries[1] =
            (AbDataEntry){text("afterCollisionVelocityX"),
                          {.kind = AB_DATA_NUMBER, .bits = host->proposed[0]}};
        host->entries[2] =
            (AbDataEntry){text("afterCollisionVelocityY"),
                          {.kind = AB_DATA_NUMBER, .bits = host->proposed[1]}};
        host->parameters = (AbDataTable){host->entries, 3, 3, NULL};
        call->response.new_collision_parameters =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &host->parameters};
    }
    return true;
}

static bool contact_velocity(void *context, AbSpan name, AbDeferredVelocity **output,
                             AbError *error) {
    ContactHost *host = context;
    CHECK(name.size == 5 && memcmp(name.data, "first", 5) == 0);
    ++host->offers;
    if (host->fail_lookup) {
        return ab_fail(error, "Controlled contact velocity lookup failure");
    }
    *output = host->state;
    return true;
}

static bool end_callback(void *context, AbSpan first, AbSpan second, AbError *error) {
    ContactHost *host = context;
    CHECK(first.size == 5 && memcmp(first.data, "first", 5) == 0 && second.size == 6 &&
          memcmp(second.data, "second", 6) == 0);
    ++host->callbacks;
    return !host->fail_callback || ab_fail(error, "Controlled end-contact failure");
}

static bool contact_edges(void) {
    AbGameContactInput input = {.callbacks_disabled = true};
    AbGameContactHost no_host = {0};
    CHECK(!ab_game_contact_begin(NULL, no_host, NULL) &&
          ab_game_contact_begin(&input, no_host, NULL));
    input.callbacks_disabled = false;
    CHECK(ab_game_contact_begin(&input, no_host, NULL));
    AbGameContactObject first = {text("first").string, true};
    AbGameContactObject second = {text("second").string, true};
    AbWorldManifold manifold = {.normal = {1, 0}, .points = {{1, 0}, {NAN, NAN}}};
    AbContactState contact = {.flags = 7};
    input.first = &first;
    input.second = &second;
    input.manifold = &manifold;
    input.contact = &contact;
    CHECK(!ab_game_contact_begin(&input, no_host, NULL) && contact.flags == 7);
    AbDeferredVelocity state = {0};
    ContactHost host = {.flags = 6,
                        .proposed = {UINT32_C(0x3f800000), UINT32_C(0x40000000)},
                        .state = &state,
                        .fail_callback = true};
    AbGameContactHost callbacks = {&host, contact_callback, contact_velocity, NULL};
    CHECK(!ab_game_contact_begin(&input, callbacks, NULL) && contact.flags == 7 &&
          host.callbacks == 1 && !host.offers && !state.pending);
    host.fail_callback = false;
    host.fail_lookup = true;
    CHECK(!ab_game_contact_begin(&input, callbacks, NULL) && contact.flags == 3 &&
          host.offers == 1 && !state.pending);
    host.fail_lookup = false;
    AbDataTable empty = {0};
    host.glue = (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    CHECK(!ab_game_contact_begin(&input, callbacks, NULL) && contact.flags == 3 &&
          state.pending && state.velocity.x == 1 && state.velocity.y == 2);
    host.glue = (AbDataValue){0};
    CHECK(ab_game_contact_begin(&input, callbacks, NULL) && contact.flags == 3);
    host.proposed[0] = UINT32_C(0x7fc00000);
    size_t previous_offers = host.offers;
    CHECK(!ab_game_contact_begin(&input, callbacks, NULL) &&
          host.offers == previous_offers && state.velocity.x == 1 &&
          state.velocity.y == 2);
    manifold.normal.x = NAN;
    size_t previous_callbacks = host.callbacks;
    CHECK(!ab_game_contact_begin(&input, callbacks, NULL) &&
          host.callbacks == previous_callbacks);
    first.script_table_present = false;
    CHECK(ab_game_contact_begin(&input, callbacks, NULL) &&
          host.callbacks == previous_callbacks);
    CHECK(ab_game_contact_end(&first, &second, NULL, NULL, NULL) &&
          ab_game_contact_end(NULL, &second, NULL, NULL, NULL) &&
          ab_game_contact_end(&first, NULL, NULL, NULL, NULL));
    first.script_table_present = true;
    CHECK(!ab_game_contact_end(&first, &second, NULL, NULL, NULL) &&
          ab_game_contact_end(&first, &second, end_callback, &host, NULL) &&
          host.callbacks == previous_callbacks + 1 && host.offers == previous_offers &&
          contact.flags == 3);
    host.fail_callback = true;
    CHECK(!ab_game_contact_end(&first, &second, end_callback, &host, NULL) &&
          host.callbacks == previous_callbacks + 2);
    return true;
}

static bool edge_checks(void) {
    AbDeferredVelocity state = {.velocity = {NAN, NAN}};
    CHECK(ab_collision_velocity_offer(&state, (AbVec2){3, 4}));
    CHECK(state.pending && state.velocity.x == 3 && state.velocity.y == 4);
    CHECK(ab_collision_velocity_offer(&state, (AbVec2){-4, 3}));
    CHECK(state.velocity.x == 3 && state.velocity.y == 4);
    CHECK(ab_collision_velocity_offer(&state, (AbVec2){2, 1}));
    CHECK(state.velocity.x == 2 && state.velocity.y == 1);
    AbDeferredVelocity before = state;
    CHECK(!ab_collision_velocity_offer(&state, (AbVec2){NAN, 0}));
    CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    CHECK(!ab_collision_velocity_flush(&state, NULL));
    CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    AbBodyMotion body = {
        .mass.type = AB_BODY_STATIC, .mass.linear_velocity = {4, 5}, .sleep_time = 7};
    CHECK(ab_collision_velocity_flush(&state, &body));
    CHECK(!state.pending && body.mass.linear_velocity.x == 4 &&
          body.mass.linear_velocity.y == 5 && body.sleep_time == 7 && !body.awake);
    CHECK(ab_collision_velocity_flush(&state, NULL));
    CHECK(ab_collision_velocity_offer(&state, (AbVec2){FLT_TRUE_MIN, -FLT_TRUE_MIN}));
    body.mass.type = AB_BODY_DYNAMIC;
    CHECK(ab_collision_velocity_flush(&state, &body));
    CHECK(!state.pending && !body.awake && body.sleep_time == 7 &&
          bits(body.mass.linear_velocity.x) == 1 &&
          bits(body.mass.linear_velocity.y) == UINT32_C(0x80000001));
    CHECK(!ab_collision_velocity_offer(NULL, (AbVec2){0}));
    CHECK(!ab_collision_velocity_flush(NULL, &body));
    return contact_edges();
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    bool ending = strcmp(marker, "AB_ANDROID_GAME_CONTACT_END_V1 4096\n") == 0;
    CHECK(ending || strcmp(marker, "AB_ANDROID_GAME_CONTACT_V1 4096\n") == 0);
    size_t callbacks = 0, offers = 0, changes = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[5], expected[9];
        CHECK(read_words(file, input, 5) && input[0] <= 255);
        CHECK(read_words(file, expected, 9));
        uint32_t flags = input[0];
        AbDeferredVelocity state = {
            .velocity = {from_bits(input[1]), from_bits(input[2])},
            .pending = (flags & 1) != 0};
        ContactHost host = {
            .flags = flags, .proposed = {input[3], input[4]}, .state = &state};
        AbGameContactObject first = {text("first").string, (flags & 64) != 0};
        AbGameContactObject second = {text("second").string, (flags & 128) != 0};
        AbContactState contact = {.flags = 7};
        AbWorldManifold geometry = {.normal = {1, 0}, .points = {{1, 0}}};
        AbGameContactInput request = {.first = flags & 16 ? &first : NULL,
                                      .second = flags & 32 ? &second : NULL,
                                      .manifold = &geometry,
                                      .contact = &contact,
                                      .callbacks_disabled = (flags & 8) != 0};
        if (ending) {
            CHECK(ab_game_contact_end(request.first, request.second, end_callback,
                                      &host, NULL));
        } else {
            CHECK(ab_game_contact_begin(
                &request,
                (AbGameContactHost){&host, contact_callback, contact_velocity, NULL},
                NULL));
        }
        uint32_t actual[] = {
            state.pending,    bits(state.velocity.x),   bits(state.velocity.y),
            contact.flags,    (uint32_t)host.callbacks, host.geometry[0],
            host.geometry[1], host.geometry[2],         host.geometry[3]};
        for (size_t field = 0; field < 9; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native contact %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        callbacks += host.callbacks;
        offers += host.offers;
        changes +=
            bits(state.velocity.x) != input[1] || bits(state.velocity.y) != input[2];
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, ending ? "AB game contact end reference complete\n"
                                : "AB game contact reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    if (ending) {
        CHECK(!offers && !changes);
        printf("All 4096 native end-contact cases match: %zu callbacks; "
               "contact/velocity state retained, independent of begin-disable flag.\n",
               callbacks);
        return true;
    }
    printf("All 4096 native contact-handler calls match: %zu callbacks, %zu deferred "
           "proposals, %zu velocity changes. The C handler applies callback guards, "
           "copied geometry, disable state and ordered proposal selection.\n",
           callbacks, offers, changes);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_collision_velocity_tests [NATIVE_CONTACT_TRACE]\n");
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
