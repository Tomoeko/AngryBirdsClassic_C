#include "ab/trajectory_follow.h"
#include "trajectory_follow_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Trajectory follow check at %d: %s\n", __LINE__, #value);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable objects[3];
    AbDataEntry entries[3][2];
    AbTrajectoryTrack tracks[8];
    AbTrajectoryFollow follow;
    uint32_t events[8][8], count, serial;
} TrajectoryFixture;

static const AbSpan custom_sprites[] = {{(const uint8_t *)"ALT_1", 5},
                                        {(const uint8_t *)"ALT_2", 5}};

static uint32_t role(TrajectoryFixture *fixture, const AbDataTable *object) {
    for (uint32_t index = 0; index < 3; ++index)
        if (object == fixture->objects + index)
            return index + 1;
    return UINT32_MAX;
}

static bool emit(void *context, const AbTrajectoryPoint *point, AbError *error) {
    (void)error;
    TrajectoryFixture *fixture = context;
    CHECK(fixture->count < 8);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = ++fixture->serial;
    event[1] = (uint32_t)point->layer;
    const char *const names[] = {"TRAIL_WHITE_1", "TRAIL_WHITE_2", "TRAIL_WHITE_3",
                                 "ALT_1", "ALT_2"};
    for (uint32_t index = 0; index < 5; ++index)
        if (point->sprite.size == strlen(names[index]) &&
            !memcmp(point->sprite.data, names[index], point->sprite.size))
            event[2] = index + 1;
    CHECK(event[2]);
    event[3] = trajectory_word(point->position.x);
    event[4] = trajectory_word(point->position.y);
    event[5] = trajectory_word(point->angle);
    event[6] = role(fixture, point->object);
    return true;
}

static void initialize(TrajectoryFixture *fixture) {
    *fixture = (TrajectoryFixture){0};
    for (uint32_t index = 0; index < 3; ++index) {
        fixture->entries[index][0] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = {(const uint8_t *)"x", 1}},
                          {.kind = AB_DATA_NUMBER}};
        fixture->entries[index][1] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = {(const uint8_t *)"y", 1}},
                          {.kind = AB_DATA_NUMBER}};
        fixture->objects[index] = (AbDataTable){fixture->entries[index], 2, 2, NULL};
    }
    fixture->follow = (AbTrajectoryFollow){
        .tracks = fixture->tracks, .capacity = 8, .context = fixture, .emit = emit};
}

static bool execute(TrajectoryFixture *fixture, const uint32_t input[12]) {
    for (uint32_t index = 0; index < 3; ++index) {
        fixture->entries[index][0].value.bits = input[4 + index * 2];
        fixture->entries[index][1].value.bits = input[5 + index * 2];
    }
    fixture->follow.emit = input[10] ? emit : NULL;
    fixture->count = 0;
    memset(fixture->events, 0, sizeof(fixture->events));
    if (!input[0])
        return ab_trajectory_follow_add(
            &fixture->follow, fixture->objects + input[1] - 1,
            input[3] ? custom_sprites : NULL, input[3] == 1 ? 2u : 1u,
            (input[2] & 1u) != 0, (input[2] & 2u) != 0, NULL);
    if (input[0] == 1)
        return ab_trajectory_follow_remove(&fixture->follow,
                                           fixture->objects + input[1] - 1, NULL);
    return ab_trajectory_follow_update(&fixture->follow, NULL);
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    return true;
}

static bool same_words(const uint32_t *actual, const uint32_t *expected, size_t count,
                       uint32_t scene, uint32_t step, const char *label) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "%s scene %u step %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    label, scene, step, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool corpus(FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_TRAJECTORY_FOLLOW_V1 8192\n"));
    size_t events = 0, failures = 0;
    for (uint32_t scene = 0; scene < TRAJECTORY_SCENES; ++scene) {
        TrajectoryFixture fixture;
        initialize(&fixture);
        for (uint32_t step = 0; step < TRAJECTORY_STEPS; ++step) {
            uint32_t input[12], actual_input[12], output[3], tracks[56] = {0};
            CHECK(read_words(file, input, 12) && read_words(file, output, 3));
            trajectory_command(scene, step, (uint32_t)fixture.follow.count,
                               actual_input);
            CHECK(same_words(actual_input, input, 12, scene, step, "input"));
            bool okay = execute(&fixture, input);
            uint32_t actual_output[] = {!okay, (uint32_t)fixture.follow.count,
                                        fixture.count};
            CHECK(same_words(actual_output, output, 3, scene, step, "output"));
            CHECK(read_words(file, tracks, fixture.follow.count * 7));
            for (size_t index = 0; index < fixture.follow.count; ++index) {
                AbTrajectoryTrack *track = fixture.tracks + index;
                uint32_t actual[] = {role(&fixture, track->object),
                                     trajectory_word(track->last_position.x),
                                     trajectory_word(track->last_position.y),
                                     (uint32_t)track->latest_sprite,
                                     (uint32_t)track->sprite_count,
                                     track->world_coordinates,
                                     track->velocity_angle};
                CHECK(same_words(actual, tracks + index * 7, 7, scene, step, "track"));
            }
            for (uint32_t index = 0; index < fixture.count; ++index) {
                uint32_t expected[8];
                CHECK(read_words(file, expected, 8));
                CHECK(same_words(fixture.events[index], expected, 8, scene, step,
                                 "point"));
            }
            events += fixture.count;
            failures += !okay;
        }
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB trajectory follow reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 native follow operations, %zu point emissions and %zu ordered "
           "failures "
           "match\n",
           events, failures);
    return true;
}

static bool edge_checks(void) {
    TrajectoryFixture fixture;
    initialize(&fixture);
    CHECK(ab_trajectory_follow_add(&fixture.follow, fixture.objects, NULL, 0, false,
                                   false, NULL));
    CHECK(ab_trajectory_follow_add(&fixture.follow, fixture.objects + 1, NULL, 0, false,
                                   false, NULL));
    CHECK(ab_trajectory_follow_add(&fixture.follow, fixture.objects, NULL, 0, false,
                                   false, NULL));
    CHECK(ab_trajectory_follow_remove(&fixture.follow, fixture.objects, NULL));
    CHECK(fixture.follow.count == 2 && fixture.tracks[1].object == fixture.objects + 1);
    CHECK(ab_trajectory_follow_remove(&fixture.follow, fixture.objects + 2, NULL));
    CHECK(fixture.follow.count == 1 && fixture.tracks[0].object == fixture.objects);
    CHECK(ab_trajectory_follow_remove(&fixture.follow, NULL, NULL));
    CHECK(!fixture.follow.count &&
          ab_trajectory_follow_remove(&fixture.follow, NULL, NULL));
    fixture.entries[0][0].value.bits = trajectory_word(2);
    fixture.entries[0][1].value.bits = trajectory_word(3);
    CHECK(ab_trajectory_follow_add(&fixture.follow, fixture.objects, NULL, 0, true,
                                   true, NULL));
    CHECK(fixture.tracks[0].last_position.x == 40 &&
          fixture.tracks[0].last_position.y == 60);
    CHECK(ab_trajectory_follow_update(&fixture.follow, NULL));
    CHECK(fixture.count == 1 && fixture.tracks[0].latest_sprite == 1 &&
          fixture.tracks[0].last_position.x == 2);
    fixture.follow.emit = NULL;
    fixture.entries[0][0].value.bits = trajectory_word(100);
    CHECK(!ab_trajectory_follow_update(&fixture.follow, NULL));
    CHECK(fixture.tracks[0].latest_sprite == 2 &&
          fixture.tracks[0].last_position.x == 2);
    fixture.entries[0][0].value.bits = UINT32_C(0x7fc00000);
    CHECK(!ab_trajectory_follow_update(&fixture.follow, NULL));
    CHECK(fixture.tracks[0].latest_sprite == 2 && fixture.follow.count == 1);
    CHECK(!ab_trajectory_follow_add(&fixture.follow, fixture.objects, custom_sprites, 0,
                                    false, false, NULL));
    CHECK(fixture.follow.count == 1);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return edge_checks() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = corpus(file);
    fclose(file);
    return okay ? 0 : 1;
}
