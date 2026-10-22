#include "ab/game_contact_routes.h"
#include "game_contact_route_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Contact route failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbGameContactRoutes routes;
    CHECK(ab_game_contact_routes_initialize(&routes));
    CHECK(
        routes.handlers[AB_GAME_CONTACT_BEGIN][AB_COLLIDER_EAGLE][AB_COLLIDER_PORTAL] ==
            AB_GAME_CONTACT_PORTAL_BEGIN &&
        routes.handlers[AB_GAME_CONTACT_BEGIN][AB_COLLIDER_BIRD][AB_COLLIDER_BUBBLE] ==
            AB_GAME_CONTACT_BUBBLE_BEGIN &&
        routes.handlers[AB_GAME_CONTACT_END][AB_COLLIDER_BIRD][AB_COLLIDER_BUBBLE] ==
            AB_GAME_CONTACT_NO_HANDLER &&
        routes.handlers[AB_GAME_CONTACT_END][AB_COLLIDER_BASIC]
                       [AB_COLLIDER_INVISIBLE] == AB_GAME_CONTACT_NO_HANDLER);
    CHECK(ab_game_contact_routes_set(&routes, AB_GAME_CONTACT_PRE_SOLVE, 2, 4,
                                     UINT32_MAX) &&
          ab_game_contact_routes_set(&routes, AB_GAME_CONTACT_END, AB_COLLIDER_BIRD,
                                     AB_COLLIDER_BUBBLE, UINT32_MAX) &&
          ab_game_contact_routes_setup(&routes) &&
          routes.handlers[AB_GAME_CONTACT_PRE_SOLVE][2][4] == UINT32_MAX &&
          routes.handlers[AB_GAME_CONTACT_PRE_SOLVE][4][2] == UINT32_MAX &&
          routes.handlers[AB_GAME_CONTACT_END][AB_COLLIDER_BIRD][AB_COLLIDER_BUBBLE] ==
              UINT32_MAX &&
          routes.handlers[AB_GAME_CONTACT_END][AB_COLLIDER_BUBBLE][AB_COLLIDER_BIRD] ==
              UINT32_MAX);
    AbGameContactRoutes before = routes;
    CHECK(!ab_game_contact_routes_initialize(NULL) &&
          !ab_game_contact_routes_setup(NULL) &&
          !ab_game_contact_routes_set(NULL, AB_GAME_CONTACT_BEGIN, 0, 0, 0) &&
          !ab_game_contact_routes_set(&routes, (AbGameContactStage)-1, 0, 0, 0) &&
          !ab_game_contact_routes_set(&routes, AB_GAME_CONTACT_STAGES, 0, 0, 0) &&
          !ab_game_contact_routes_set(&routes, AB_GAME_CONTACT_BEGIN, 12, 0, 0) &&
          !ab_game_contact_routes_set(&routes, AB_GAME_CONTACT_BEGIN, 0, 12, 0) &&
          memcmp(&routes, &before, sizeof(routes)) == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool compare(FILE *file, const AbGameContactRoutes *routes, uint32_t sample,
                    const char *stage) {
    for (size_t kind = 0; kind < AB_GAME_CONTACT_STAGES; ++kind) {
        for (size_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
            uint32_t expected[12];
            CHECK(read_words(file, expected, 12));
            for (size_t second = 0; second < AB_GAME_COLLIDER_TYPES; ++second) {
                uint32_t actual = routes->handlers[kind][first][second];
                if (actual != expected[second]) {
                    fprintf(stderr,
                            "Route %" PRIu32 " %s kind %zu pair %zu/%zu: %" PRIu32
                            " != %" PRIu32 "\n",
                            sample, stage, kind, first, second, actual,
                            expected[second]);
                    return false;
                }
            }
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_CONTACT_ROUTES_V1 512\n") == 0);
    for (uint32_t sample = 0; sample < 512; ++sample) {
        uint32_t input[436], generated[436];
        game_contact_route_case(sample, generated);
        CHECK(read_words(file, input, 436) &&
              memcmp(input, generated, sizeof(input)) == 0);
        AbGameContactRoutes routes;
        memcpy(routes.handlers, input, sizeof(routes.handlers));
        CHECK(ab_game_contact_routes_setup(&routes) &&
              compare(file, &routes, sample, "setup"));
        CHECK(ab_game_contact_routes_set(&routes, (AbGameContactStage)input[432],
                                         input[433], input[434], input[435]) &&
              compare(file, &routes, sample, "setter"));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game contact routes reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 512 native contact-handler setups and symmetric updates match: "
         "442,368 handler tokens, including preserved entries and pre-solve tables.");
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
