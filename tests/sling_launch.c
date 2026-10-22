#include "ab/slingshot_launch.h"
#include "ab/slingshot.h"
#include "sling_launch_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Sling launch check at %d: %s\n", __LINE__, #value);       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[20], events[9][21], event_count, release_events[2], release_count;
    SlingLaunchFixture fixture;
    AbSlingLaunchState state;
    AbShotBird birds[2];
    AbVec2 direction, band, last_impulse;
} LaunchCase;

static void fixture_to_runtime(LaunchCase *test) {
    SlingLaunchFixture *fixture = &test->fixture;
    test->state = (AbSlingLaunchState){.dragged = test->birds + fixture->role,
                                       .default_force = fixture->default_force,
                                       .physics_scale = fixture->scale,
                                       .eagle_bait_is_true = fixture->eagle,
                                       .special_mode_truthy = fixture->special};
    test->birds[0].name = "bird_A";
    test->birds[1].name = "bird_B";
    test->birds[0].mass = fixture->mass[0];
    test->birds[1].mass = fixture->mass[1];
    test->direction = (AbVec2){fixture->direction[0], fixture->direction[1]};
    test->band = (AbVec2){fixture->band[0], fixture->band[1]};
}

static void runtime_to_fixture(LaunchCase *test) {
    SlingLaunchFixture *fixture = &test->fixture;
    fixture->role = test->state.dragged == test->birds ? 0 : 1;
    fixture->mass[0] = test->birds[0].mass;
    fixture->mass[1] = test->birds[1].mass;
    fixture->default_force = test->state.default_force;
    fixture->scale = test->state.physics_scale;
    fixture->eagle = test->state.eagle_bait_is_true;
    fixture->special = test->state.special_mode_truthy;
    fixture->direction[0] = test->direction.x;
    fixture->direction[1] = test->direction.y;
    fixture->band[0] = test->band.x;
    fixture->band[1] = test->band.y;
}

static bool launch_host(void *context, AbSlingLaunchCall *call, AbError *error) {
    LaunchCase *test = context;
    if ((uint32_t)call->operation == test->input[18])
        return ab_fail(error, "Unavailable controlled launch service");
    CHECK(test->event_count < 9);
    runtime_to_fixture(test);
    uint32_t *event = test->events[test->event_count++];
    event[0] = (uint32_t)call->operation;
    if (call->name)
        event[1] = strcmp(call->name, "bird_A") == 0 ? 1u : 2u;
    float impulse[2] = {0};
    if (call->operation == AB_SLING_LAUNCH_WITH_IMPULSE) {
        CHECK(call->position && call->impulse);
        event[2] = sling_launch_word(call->position->x);
        event[3] = sling_launch_word(call->position->y);
        event[4] = sling_launch_word(call->impulse->x);
        event[5] = sling_launch_word(call->impulse->y);
        event[19] = call->position == &test->direction;
        event[20] =
            call->impulse != call->position && call->impulse != &test->direction;
        impulse[0] = call->impulse->x;
        impulse[1] = call->impulse->y;
    }
    sling_launch_snapshot(&test->fixture, event + 6);
    sling_launch_effect(&test->fixture, (uint32_t)call->operation, impulse);
    fixture_to_runtime(test);
    switch (call->operation) {
        case AB_SLING_LAUNCH_DIRECTION:
            call->direction = &test->direction;
            break;
        case AB_SLING_LAUNCH_DISTANCE:
            call->number = sling_launch_number(test->input[2]);
            break;
        case AB_SLING_LAUNCH_STRETCH:
            call->number = sling_launch_number(test->input[7]);
            break;
        case AB_SLING_LAUNCH_FORCE:
            call->force_present = test->input[17] == 2;
            call->number = sling_launch_number(test->input[9]);
            break;
        case AB_SLING_LAUNCH_POWER:
            call->number = sling_launch_number(test->input[8]);
            break;
        case AB_SLING_LAUNCH_POSITION:
            call->position = test->fixture.alias_band ? &test->direction : &test->band;
            break;
        case AB_SLING_LAUNCH_WITH_IMPULSE:
            *call->impulse = (AbVec2){impulse[0], impulse[1]};
            test->last_impulse = *call->impulse;
            break;
        case AB_SLING_LAUNCH_BACKWARDS:
        case AB_SLING_LAUNCH_SHOT_COUNT:
            break;
    }
    return true;
}

static bool release_host(void *context, AbSlingReleaseCall *call, AbError *error) {
    LaunchCase *test = context;
    if ((uint32_t)call->operation == test->input[19])
        return ab_fail(error, "Unavailable controlled release service");
    CHECK(test->release_count < 2);
    test->release_events[test->release_count++] = (uint32_t)call->operation;
    if (call->operation == AB_SLING_RELEASE_CHECK)
        call->stretched = test->input[14] >= 2;
    return true;
}

static bool read_word(FILE *file, uint32_t actual, uint32_t sample) {
    uint32_t expected;
    CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
    if (actual != expected) {
        fprintf(stderr, "Sling launch %u: %08" PRIx32 " != %08" PRIx32 "\n", sample,
                actual, expected);
        return false;
    }
    return true;
}

static bool run_case(LaunchCase *test, bool *success, bool *released) {
    test->fixture = sling_launch_fixture(test->input);
    fixture_to_runtime(test);
    *success =
        ab_slingshot_launch(&test->state, test->input[15] & 1 ? &test->direction : NULL,
                            launch_host, test, NULL);
    *released = ab_slingshot_release(release_host, test, NULL);
    runtime_to_fixture(test);
    return true;
}

static bool corpus(FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_SLING_LAUNCH_HOST_V1 4096\n"));
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        LaunchCase test = {0};
        sling_launch_case(sample, test.input);
        bool success, released;
        CHECK(run_case(&test, &success, &released));
        for (size_t index = 0; index < 20; ++index)
            CHECK(read_word(file, test.input[index], sample));
        uint32_t prefix[] = {success, released, test.event_count, test.release_count};
        for (size_t index = 0; index < 4; ++index)
            CHECK(read_word(file, prefix[index], sample));
        for (size_t event = 0; event < test.event_count; ++event)
            for (size_t index = 0; index < 21; ++index)
                CHECK(read_word(file, test.events[event][index], sample));
        uint32_t snapshot[13];
        sling_launch_snapshot(&test.fixture, snapshot);
        for (size_t index = 0; index < 13; ++index)
            CHECK(read_word(file, snapshot[index], sample));
        CHECK(read_word(file, sling_launch_word(test.last_impulse.x), sample) &&
              read_word(file, sling_launch_word(test.last_impulse.y), sample));
        for (size_t index = 0; index < 2; ++index)
            CHECK(read_word(file, test.release_events[index], sample));
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB sling launch host reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original launch/release scenarios and ordered mutable boundaries "
         "match");
    return true;
}

static bool controls(void) {
    CHECK(!ab_slingshot_launch(NULL, NULL, launch_host, NULL, NULL));
    CHECK(!ab_slingshot_release(NULL, NULL, NULL));
    LaunchCase test = {0};
    sling_launch_case(1, test.input);
    test.input[16] = test.input[18] = test.input[19] = 0;
    test.input[15] = 1;
    bool success, released;
    CHECK(run_case(&test, &success, &released) && success && released &&
          test.fixture.shots == 11);
    AbSlingLaunchInput input = {.bird_present = true,
                                .stretch = sling_launch_number(test.input[7]),
                                .launch_force = test.input[17] == 2
                                                    ? sling_launch_number(test.input[9])
                                                    : test.fixture.default_force,
                                .physics_scale = test.fixture.scale,
                                .bird_mass = test.fixture.mass[0],
                                .slingshot_power = sling_launch_number(test.input[8])};
    AbVec2 impulse;
    CHECK(ab_slingshot_launch_impulse(&input, &test.direction, &impulse) &&
          sling_launch_word(impulse.x) == sling_launch_word(test.last_impulse.x) &&
          sling_launch_word(impulse.y) == sling_launch_word(test.last_impulse.y));
    test = (LaunchCase){0};
    sling_launch_case(3, test.input);
    test.input[18] = AB_SLING_LAUNCH_FORCE;
    CHECK(run_case(&test, &success, &released) && !success && test.fixture.shots == 11);
    for (size_t event = 0; event < test.event_count; ++event)
        CHECK(test.events[event][0] < AB_SLING_LAUNCH_FORCE);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return controls() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = corpus(file);
    fclose(file);
    return okay ? 0 : 1;
}
