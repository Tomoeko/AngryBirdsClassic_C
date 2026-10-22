#include "ab/bird_remove.h"
#include "bird_remove_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Bird removal check failed at %d: %s\n", __LINE__,         \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[BIRD_REMOVE_INPUT_WORDS];
    AbBirdRemoveState state;
    AbBirdRemoveHost host;
    AbDataTable tables[BIRD_REMOVE_TABLE_COUNT];
    AbDataEntry entries[BIRD_REMOVE_TABLE_COUNT][8];
    uint32_t events[BIRD_REMOVE_MAX_EVENTS][BIRD_REMOVE_EVENT_WORDS];
    size_t event_count, definitions;
} BirdRemoveFixture;

static const char *const strings[] = {
    "Bird",           "Renamed",      "Hat",       "Other",  "hook",    "removed",
    "bird_destroyed", "bubble_sound", "particles", "truthy", "resolved"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(uint32_t bits) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits};
}

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value};
}

static AbDataValue table(BirdRemoveFixture *fixture, BirdRemoveTable index) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + index};
}

static AbDataValue get(AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool set(BirdRemoveFixture *fixture, BirdRemoveTable index, const char *name,
                AbDataValue value) {
    AbDataTable *destination = fixture->tables + index;
    AbDataValue key = text(name);
    for (size_t item = 0; item < destination->count; ++item) {
        AbDataEntry *entry = destination->entries + item;
        if (entry->key.string.size == key.string.size &&
            !memcmp(entry->key.string.data, key.string.data, key.string.size)) {
            entry->value = value;
            return true;
        }
    }
    CHECK(destination->count < destination->capacity);
    destination->entries[destination->count++] = (AbDataEntry){key, value};
    return true;
}

static AbDataValue scalar(BirdRemoveFixture *fixture, uint32_t kind) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return boolean(kind == 2);
    }
    if (kind == 3) {
        return number(0);
    }
    return kind == 4 ? text("removed") : table(fixture, BIRD_REMOVE_PARTICLES);
}

static bool encode(BirdRemoveFixture *fixture, AbDataValue value, uint32_t output[2]) {
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN) {
        output[1] = value.bits != 0;
    } else if (value.kind == AB_DATA_NUMBER) {
        output[0] = 3;
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        output[0] = 4;
        for (size_t index = 0; index < sizeof(strings) / sizeof(*strings); ++index) {
            if (value.string.size == strlen(strings[index]) &&
                !memcmp(value.string.data, strings[index], value.string.size)) {
                output[1] = (uint32_t)index + 1u;
                return true;
            }
        }
        CHECK(false);
    } else if (value.kind == AB_DATA_TABLE) {
        output[0] = 5;
        for (size_t index = 0; index < BIRD_REMOVE_TABLE_COUNT; ++index) {
            if (value.table == fixture->tables + index) {
                output[1] = (uint32_t)index + 201u;
                return true;
            }
        }
        CHECK(false);
    } else {
        CHECK(value.kind == AB_DATA_NIL);
    }
    return true;
}

static bool snapshot(BirdRemoveFixture *fixture,
                     uint32_t output[BIRD_REMOVE_STATE_WORDS]) {
    AbDataValue values[] = {get(fixture->tables + BIRD_REMOVE_BIRD, "name"),
                            get(fixture->tables + BIRD_REMOVE_HAT_BODY, "defence"),
                            get(fixture->tables + BIRD_REMOVE_HAT_BODY, "strength"),
                            fixture->state.current_bird_name,
                            fixture->state.flying_bird,
                            fixture->state.specialty_available,
                            fixture->state.objects,
                            fixture->state.birds,
                            fixture->state.other_birds,
                            fixture->state.hats};
    size_t next = 0;
    for (size_t index = 0; index < 10; ++index) {
        CHECK(encode(fixture, values[index], output + next));
        next += 2;
    }
    const BirdRemoveTable maps[] = {BIRD_REMOVE_WORLD_A, BIRD_REMOVE_DYNAMIC_A,
                                    BIRD_REMOVE_BIRDS_A, BIRD_REMOVE_OTHERS_A,
                                    BIRD_REMOVE_WORLD_B, BIRD_REMOVE_DYNAMIC_B,
                                    BIRD_REMOVE_BIRDS_B, BIRD_REMOVE_OTHERS_B};
    for (size_t index = 0; index < 8; ++index) {
        CHECK(
            encode(fixture, get(fixture->tables + maps[index], "Bird"), output + next));
        CHECK(encode(fixture, get(fixture->tables + maps[index], "Renamed"),
                     output + next + 2));
        next += 4;
    }
    for (size_t index = 0; index < 2; ++index) {
        BirdRemoveTable container =
            index ? BIRD_REMOVE_OBJECTS_B : BIRD_REMOVE_OBJECTS_A;
        CHECK(
            encode(fixture, get(fixture->tables + container, "world"), output + next));
        CHECK(encode(fixture, get(fixture->tables + container, "dynamic"),
                     output + next + 2));
        next += 4;
    }
    CHECK(next == BIRD_REMOVE_STATE_WORDS);
    return true;
}

static bool audio_two(void *context, AbAudioPlay request, AbError *error);

static void bind_maps(BirdRemoveFixture *fixture, bool second) {
    fixture->state.objects =
        table(fixture, second ? BIRD_REMOVE_OBJECTS_B : BIRD_REMOVE_OBJECTS_A);
    fixture->state.birds =
        fixture->input[2] == 13
            ? boolean(false)
            : table(fixture, second ? BIRD_REMOVE_BIRDS_B : BIRD_REMOVE_BIRDS_A);
    fixture->state.other_birds =
        fixture->input[2] == 14
            ? boolean(false)
            : table(fixture, second ? BIRD_REMOVE_OTHERS_B : BIRD_REMOVE_OTHERS_A);
}

static bool mutate(BirdRemoveFixture *fixture, uint32_t kind) {
    uint32_t flags = fixture->input[1];
    if (flags & 256u) {
        if (kind == 1 || kind == 3 || kind == 4) {
            CHECK(set(fixture, BIRD_REMOVE_BIRD, "name",
                      text(kind == 3 ? "Bird" : "Renamed")));
        } else if (kind == 6 || kind == 11) {
            bind_maps(fixture, kind == 6);
        } else if (kind == 9) {
            fixture->host.audio = audio_two;
        }
    }
    if (kind == 6 && (flags & 128u)) {
        fixture->state.current_bird_name =
            get(fixture->tables + BIRD_REMOVE_BIRD, "name");
        fixture->state.flying_bird = table(fixture, BIRD_REMOVE_BIRD);
    }
    return true;
}

static bool event(BirdRemoveFixture *fixture, uint32_t kind, uint32_t pc,
                  uint32_t variant, const AbDataValue *arguments, size_t count,
                  uint32_t missing, AbError *error) {
    if (missing && fixture->input[2] == missing) {
        return ab_fail(error, "Controlled missing bird-removal function");
    }
    CHECK(count <= 4 && fixture->event_count < BIRD_REMOVE_MAX_EVENTS);
    uint32_t *output = fixture->events[fixture->event_count++];
    output[0] = pc;
    output[1] = kind;
    output[2] = variant;
    output[3] = (uint32_t)count;
    for (size_t index = 0; index < count; ++index) {
        CHECK(encode(fixture, arguments[index], output + 4 + index * 2));
    }
    return snapshot(fixture, output + 12) && mutate(fixture, kind);
}

static bool destroy(void *context, AbDataValue script, AbError *error) {
    return event(context, 1, 13, 0, &script, 1, 1, error);
}

static bool definition(void *context, AbDataValue name, AbDataValue *output,
                       AbError *error) {
    BirdRemoveFixture *fixture = context;
    uint32_t pc = fixture->definitions++ ? 91u : 16u;
    if (!event(fixture, 2, pc, 0, &name, 1, 2, error)) {
        return false;
    }
    *output = pc == 91 && fixture->input[2] == 15
                  ? boolean(false)
                  : table(fixture, BIRD_REMOVE_DEFINITION);
    return true;
}

static bool hook(void *context, AbDataValue name, AbDataValue object, AbError *error) {
    CHECK(name.kind == AB_DATA_STRING && name.string.size == 4 &&
          !memcmp(name.string.data, "hook", 4));
    return event(context, 3, 27, 0, &object, 1, 3, error);
}

static bool particles(void *context, AbDataValue script, AbDataValue *output,
                      AbError *error) {
    BirdRemoveFixture *fixture = context;
    AbDataValue arguments[] = {script, text("particles")};
    if (!event(fixture, 4, 58, 0, arguments, 2, 5, error)) {
        return false;
    }
    *output = fixture->input[1] & 64u ? table(fixture, BIRD_REMOVE_PARTICLES)
                                      : boolean(false);
    return true;
}

static bool emit(void *context, AbDataValue name, AbDataValue descriptor, bool first,
                 bool second, AbError *error) {
    AbDataValue arguments[] = {name, descriptor, boolean(first), boolean(second)};
    return event(context, 5, 69, 0, arguments, 4, 6, error);
}

static bool camera(void *context, AbDataValue receiver, AbDataValue object,
                   AbError *error) {
    AbDataValue arguments[] = {receiver, object};
    return event(context, 6, 73, 0, arguments, 2, 7, error);
}

static bool volume(void *context, AbDataValue receiver, AbDataValue object,
                   AbDataValue *output, AbError *error) {
    BirdRemoveFixture *fixture = context;
    AbDataValue arguments[] = {receiver, object};
    if (!event(fixture, 8, 104, 0, arguments, 2, 8, error)) {
        return false;
    }
    *output = number(fixture->input[4]);
    return true;
}

static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error) {
    if (!event(context, 9, 108, 0, &sound, 1, 9, error)) {
        return false;
    }
    *output = text("resolved");
    return true;
}

static bool audio(void *context, AbAudioPlay request, uint32_t variant,
                  AbError *error) {
    CHECK(request.channel.kind == AB_DATA_NIL);
    AbDataValue arguments[] = {request.name, request.volume, request.loop};
    return event(context, 10, 111, variant, arguments, 3, 10, error);
}

static bool audio_one(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 1, error);
}

static bool audio_two(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 2, error);
}

static bool remove_bird(void *context, AbDataValue name, AbError *error) {
    return event(context, 11, 114, 0, &name, 1, 11, error);
}

static bool prepare(BirdRemoveFixture *fixture) {
    for (size_t index = 0; index < BIRD_REMOVE_TABLE_COUNT; ++index) {
        fixture->tables[index] = (AbDataTable){fixture->entries[index], 0, 8, NULL};
    }
    uint32_t flags = fixture->input[1], missing = fixture->input[2];
    CHECK(set(fixture, BIRD_REMOVE_BIRD, "name", text("Bird")));
    CHECK(set(fixture, BIRD_REMOVE_BIRD, "inBubble", boolean((flags & 8u) != 0)));
    CHECK(set(fixture, BIRD_REMOVE_BIRD, "gameObjectScript",
              flags & 1u ? table(fixture, BIRD_REMOVE_SCRIPT) : (AbDataValue){0}));
    CHECK(set(fixture, BIRD_REMOVE_DEFINITION, "onBirdRemoved",
              flags & 2u ? text("hook") : boolean(false)));
    CHECK(set(fixture, BIRD_REMOVE_DEFINITION, "destroyedInsideBubbleSound",
              scalar(fixture, flags & 64u ? fixture->input[3] : 1u)));
    CHECK(set(fixture, BIRD_REMOVE_HATS, "Bird",
              flags & 4u ? table(fixture, BIRD_REMOVE_HAT_ENTRY) : (AbDataValue){0}));
    CHECK(set(fixture, BIRD_REMOVE_HATS, "Renamed",
              flags & 4u ? table(fixture, BIRD_REMOVE_HAT_ENTRY) : (AbDataValue){0}));
    CHECK(set(fixture, BIRD_REMOVE_HAT_ENTRY, "hat", text("Hat")));
    CHECK(set(fixture, BIRD_REMOVE_HAT_BODY, "defence", number(UINT32_C(0x41200000))));
    CHECK(set(fixture, BIRD_REMOVE_HAT_BODY, "strength", number(UINT32_C(0x41a00000))));
    for (uint32_t group = 0; group < 2; ++group) {
        uint32_t start = group ? BIRD_REMOVE_WORLD_B : BIRD_REMOVE_WORLD_A;
        for (uint32_t index = 0; index < 4; ++index) {
            BirdRemoveTable map = (BirdRemoveTable)(start + index);
            CHECK(set(fixture, map, "Bird", table(fixture, BIRD_REMOVE_BIRD)));
            CHECK(set(fixture, map, "Renamed", table(fixture, BIRD_REMOVE_BIRD)));
            if (!index) {
                CHECK(set(fixture, map, "Hat", table(fixture, BIRD_REMOVE_HAT_BODY)));
            }
        }
        BirdRemoveTable container =
            group ? BIRD_REMOVE_OBJECTS_B : BIRD_REMOVE_OBJECTS_A;
        CHECK(set(fixture, container, "world", table(fixture, (BirdRemoveTable)start)));
        CHECK(set(fixture, container, "dynamic",
                  missing == 12 ? boolean(false)
                                : table(fixture, (BirdRemoveTable)(start + 1u))));
    }
    CHECK(set(fixture, BIRD_REMOVE_PARTICLES, "removal",
              scalar(fixture, fixture->input[3])));
    fixture->state = (AbBirdRemoveState){
        .hats = missing == 4 ? boolean(false) : table(fixture, BIRD_REMOVE_HATS),
        .camera = table(fixture, BIRD_REMOVE_CAMERA),
        .level_effect =
            flags & 32u ? table(fixture, BIRD_REMOVE_EFFECT) : boolean(false),
        .current_bird_name = text(flags & 128u ? "Other" : "Bird"),
        .flying_bird =
            table(fixture, flags & 128u ? BIRD_REMOVE_HAT_BODY : BIRD_REMOVE_BIRD),
        .specialty_available = boolean(true)};
    bind_maps(fixture, false);
    fixture->host =
        (AbBirdRemoveHost){fixture, destroy, definition, hook,      particles,  emit,
                           camera,  volume,  audio_name, audio_one, remove_bird};
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Bird removal case %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_BIRD_REMOVE_V1 1024\n"));
    size_t matched = 0, calls = 0, failures = 0;
    for (uint32_t sample = 0; sample < 1024; ++sample) {
        BirdRemoveFixture fixture = {0};
        bird_remove_case(sample, fixture.input);
        CHECK(prepare(&fixture) &&
              words(file, fixture.input, BIRD_REMOVE_INPUT_WORDS, sample));
        AbError error = {0};
        uint32_t failed = !ab_bird_remove(table(&fixture, BIRD_REMOVE_BIRD),
                                          scalar(&fixture, fixture.input[5]),
                                          &fixture.state, &fixture.host, &error);
        CHECK(words(file, &failed, 1, sample));
        uint32_t state[BIRD_REMOVE_STATE_WORDS];
        CHECK(snapshot(&fixture, state) &&
              words(file, state, BIRD_REMOVE_STATE_WORDS, sample));
        uint32_t count = (uint32_t)fixture.event_count;
        CHECK(words(file, &count, 1, sample));
        for (size_t event = 0; event < fixture.event_count; ++event) {
            CHECK(words(file, fixture.events[event], BIRD_REMOVE_EVENT_WORDS, sample));
        }
        matched += BIRD_REMOVE_INPUT_WORDS + 2 + BIRD_REMOVE_STATE_WORDS +
                   BIRD_REMOVE_EVENT_WORDS * fixture.event_count;
        calls += fixture.event_count;
        failures += failed;
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB bird remove reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All 1024 complete original bird removals match: %zu words, %zu checked "
           "boundaries, %zu partial failures.\n",
           matched, calls, failures);
    return true;
}

static bool controls(void) {
    const uint32_t samples[] = {0, 511};
    for (size_t index = 0; index < 2; ++index) {
        BirdRemoveFixture fixture = {0};
        bird_remove_case(samples[index], fixture.input);
        CHECK(prepare(&fixture));
        CHECK(ab_bird_remove(table(&fixture, BIRD_REMOVE_BIRD),
                             scalar(&fixture, fixture.input[5]), &fixture.state,
                             &fixture.host, NULL));
        CHECK(get(fixture.tables + BIRD_REMOVE_WORLD_A, "Bird").kind == AB_DATA_NIL &&
              get(fixture.tables + BIRD_REMOVE_DYNAMIC_A, "Bird").kind == AB_DATA_NIL &&
              get(fixture.tables + BIRD_REMOVE_BIRDS_A, "Bird").kind == AB_DATA_NIL &&
              get(fixture.tables + BIRD_REMOVE_OTHERS_A, "Bird").kind == AB_DATA_NIL);
        CHECK(get(fixture.tables + BIRD_REMOVE_WORLD_A, "Renamed").kind ==
                  AB_DATA_TABLE &&
              fixture.state.flying_bird.kind == AB_DATA_NIL &&
              fixture.state.specialty_available.kind == AB_DATA_BOOLEAN &&
              !fixture.state.specialty_available.bits);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return controls() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
