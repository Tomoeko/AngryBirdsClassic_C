#include "ab/game_contact_routes.h"
#include "game_contact_dispatch_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Contact dispatch failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t output[5];
    size_t contact;
    const AbManifold *previous;
    bool fail;
} Host;

static bool dispatch(void *context, const AbGameContactDispatchCall *call,
                     AbError *error) {
    Host *host = context;
    CHECK(!host->output[0] && call->previous == host->previous);
    host->output[0] = 1;
    host->output[1] = call->handler;
    host->output[2] = call->contact == host->contact;
    host->output[3] = call->previous != NULL;
    host->output[4] = (uint32_t)call->stage;
    return !host->fail || ab_fail(error, "Controlled dispatch failure");
}

static bool edge_checks(void) {
    AbGameContactDispatchInput input = {.stage = AB_GAME_CONTACT_BEGIN,
                                        .begin_disabled = true};
    CHECK(!ab_game_contact_dispatch(NULL, NULL, NULL, NULL, NULL) &&
          ab_game_contact_dispatch(NULL, &input, NULL, NULL, NULL));
    input.stage = AB_GAME_CONTACT_END;
    CHECK(ab_game_contact_dispatch(NULL, &input, NULL, NULL, NULL));
    AbGameCollisionObject first = {UINT32_MAX, false}, second = {0, false};
    input.first = &first;
    CHECK(ab_game_contact_dispatch(NULL, &input, NULL, NULL, NULL));
    input.second = &second;
    CHECK(!ab_game_contact_dispatch(NULL, &input, NULL, NULL, NULL));
    AbGameContactRoutes routes = {0};
    CHECK(!ab_game_contact_dispatch(&routes, &input, NULL, NULL, NULL));
    first.collider = 0;
    CHECK(ab_game_contact_dispatch(&routes, &input, NULL, NULL, NULL));
    routes.handlers[AB_GAME_CONTACT_END][0][0] = UINT32_MAX;
    CHECK(!ab_game_contact_dispatch(&routes, &input, NULL, NULL, NULL));
    input.contact = SIZE_MAX;
    AbManifold previous = {0};
    input.previous = &previous;
    Host host = {.contact = SIZE_MAX, .fail = true};
    CHECK(!ab_game_contact_dispatch(&routes, &input, dispatch, &host, NULL) &&
          host.output[0] == 1 && host.output[1] == UINT32_MAX && host.output[2] == 1 &&
          host.output[3] == 0 && host.output[4] == AB_GAME_CONTACT_END);
    input.stage = AB_GAME_CONTACT_STAGES;
    CHECK(!ab_game_contact_dispatch(&routes, &input, dispatch, &host, NULL));
    input.stage = (AbGameContactStage)-1;
    CHECK(!ab_game_contact_dispatch(&routes, &input, dispatch, &host, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_CONTACT_DISPATCH_V1 4096\n") == 0);
    size_t calls[3] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[5], generated[5], expected[5];
        game_contact_dispatch_case(sample, generated);
        CHECK(read_words(file, input, 5) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 5));
        AbGameContactRoutes routes;
        for (size_t stage = 0; stage < 3; ++stage) {
            for (size_t first = 0; first < 12; ++first) {
                for (size_t second = 0; second < 12; ++second) {
                    routes.handlers[stage][first][second] =
                        (uint32_t)(stage * 144 + first * 12 + second + 1);
                }
            }
        }
        if (!(input[3] & 8u)) {
            routes.handlers[input[0]][input[1]][input[2]] = 0;
        }
        AbGameCollisionObject first = {input[1], false}, second = {input[2], false};
        AbManifold previous = {0};
        AbGameContactDispatchInput request = {.stage = (AbGameContactStage)input[0],
                                              .first = input[3] & 1u ? &first : NULL,
                                              .second = input[3] & 2u ? &second : NULL,
                                              .contact = input[4],
                                              .previous =
                                                  input[3] & 16u ? &previous : NULL,
                                              .begin_disabled = (input[3] & 4u) != 0};
        Host host = {.contact = input[4],
                     .previous = input[0] == AB_GAME_CONTACT_PRE_SOLVE
                                     ? request.previous
                                     : NULL};
        CHECK(ab_game_contact_dispatch(&routes, &request, dispatch, &host, NULL));
        if (memcmp(host.output, expected, sizeof(expected)) != 0) {
            fprintf(stderr, "Native dispatch differs in sample %" PRIu32 "\n", sample);
            return false;
        }
        calls[input[0]] += host.output[0];
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game contact dispatch reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "All 4,096 native event dispatches match: %zu begin, %zu end and %zu "
        "pre-solve calls, including every ordered pair/stage and pointer forwarding.\n",
        calls[0], calls[1], calls[2]);
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
