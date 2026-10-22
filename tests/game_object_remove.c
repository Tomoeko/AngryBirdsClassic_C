#include "ab/game_object_remove.h"
#include "game_object_remove_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Native object removal failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    WorldStorage storage;
    AbPhysicsWorld world;
    AbGameObjectCore objects[2], *birds[3];
    AbGameObjectCoreRegistry registry;
    uint32_t input[11];
} Fixture;

static const char *const names[] = {"first", "second", "missing", "",
                                    "rollA", "rollB",  "rollC"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    game_object_remove_case(sample, fixture->input);
    CHECK(ab_world_open(&fixture->world, default_definition(),
                        prepare_storage(&fixture->storage, 0, 0), NULL));
    fixture->registry.world = &fixture->world;
    CHECK(ab_string_map_open(2, 2, 4096, &fixture->registry.objects, NULL));
    CHECK(ab_string_map_open(4, 4, 4096, &fixture->registry.rolling_levels, NULL));
    for (size_t index = 0; index < 2; ++index) {
        AbBodyDefinition body = {
            .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
        fixture->objects[index] = (AbGameObjectCore){
            .name = span(names[index]),
            .rolling_sound = span(names[3u + fixture->input[index + 5u]]),
            .collider =
                index ? AB_COLLIDER_BASIC : (AbGameColliderType)fixture->input[1]};
        CHECK(ab_world_create_body(&fixture->world, &body,
                                   &fixture->objects[index].body, NULL));
        CHECK(ab_string_map_set(fixture->registry.objects, span(names[index]),
                                (uintptr_t)(fixture->objects + index), true, NULL));
    }
    for (size_t sound = 0; sound < 3; ++sound) {
        CHECK(ab_string_map_set(fixture->registry.rolling_levels,
                                span(names[sound + 4u]), fixture->input[sound + 8u],
                                true, NULL));
    }
    for (uint32_t slot = 0; slot < 3; ++slot) {
        fixture->birds[slot] =
            fixture->objects + (((fixture->input[4] >> (slot * 2u)) & 3u) - 1u);
    }
    fixture->registry.birds = fixture->birds;
    fixture->registry.bird_count = fixture->input[3];
    fixture->registry.bird_capacity = 3;
    fixture->world.locked = fixture->input[7] != 0;
    return true;
}

static void close_fixture(Fixture *fixture) {
    fixture->world.locked = false;
    ab_string_map_close(fixture->registry.objects);
    ab_string_map_close(fixture->registry.rolling_levels);
    ab_world_close(&fixture->world);
}

static bool snapshot(Fixture *fixture, uint32_t output[13]) {
    memset(output, 0, 13 * sizeof(*output));
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(fixture->registry.objects, &layout));
    output[0] = (uint32_t)layout.count;
    output[1] = (uint32_t)fixture->world.body_count;
    for (uint32_t index = 0; index < 2; ++index) {
        uintptr_t value = 0;
        bool present = false;
        CHECK(ab_string_map_get(fixture->registry.objects, span(names[index]), &value,
                                &present, NULL));
        output[2] |= present ? 1u << index : 0u;
        output[12] |= ab_world_has_body(&fixture->world, fixture->objects[index].body)
                          ? 1u << index
                          : 0u;
    }
    output[3] = (uint32_t)fixture->registry.bird_count;
    for (size_t slot = 0; slot < fixture->registry.bird_count; ++slot) {
        output[4 + slot] = (uint32_t)(fixture->birds[slot] - fixture->objects) + 1u;
    }
    CHECK(ab_string_map_layout(fixture->registry.rolling_levels, &layout));
    output[7] = (uint32_t)layout.count;
    for (size_t sound = 0; sound < 3; ++sound) {
        uintptr_t value = 0;
        bool present = false;
        CHECK(ab_string_map_get(fixture->registry.rolling_levels,
                                span(names[sound + 4u]), &value, &present, NULL) &&
              present);
        output[8 + sound] = (uint32_t)value;
    }
    output[11] = fixture->world.locked;
    return true;
}

typedef struct {
    Fixture *fixture;
    uint32_t events[6], count;
    bool fail_camera, fail_scene, invalid;
} Resources;

static void event(Resources *resources, uint32_t kind) {
    if (resources->count >= 6) {
        resources->invalid = true;
        return;
    }
    resources->events[resources->count++] = kind;
}

static bool camera(void *context, AbSpan name, AbError *error) {
    Resources *resources = context;
    CHECK(name.size == 5 && !memcmp(name.data, "first", 5));
    CHECK(!ab_world_has_body(&resources->fixture->world,
                             resources->fixture->objects[0].body));
    event(resources, 1);
    return resources->fail_camera ? ab_fail(error, "Injected camera-close failure")
                                  : true;
}

static void retain(void *context, uintptr_t handle) {
    Resources *resources = context;
    resources->invalid |= handle != 17;
    event(resources, 2);
}

static bool remove_renderable(void *context, uintptr_t handle, AbError *error) {
    Resources *resources = context;
    CHECK(handle == 17);
    event(resources, 3);
    if (resources->fail_scene) {
        return ab_fail(error, "Injected scene-removal failure");
    }
    resources->fixture->objects[0].renderable = 23;
    return true;
}

static void release(void *context, uintptr_t handle) {
    Resources *resources = context;
    resources->invalid |= handle != 17 && handle != 23;
    if (handle == 23) {
        resources->invalid |= resources->fixture->objects[0].renderable != 0;
    }
    event(resources, handle == 23 ? 4u : 6u);
}

static void destroy_object(void *context, AbGameObjectCore *object) {
    Resources *resources = context;
    uintptr_t value = 0;
    bool present = true;
    resources->invalid |= object != resources->fixture->objects ||
                          object->renderable != 0 || object->body != 0;
    resources->invalid |= !ab_string_map_get(resources->fixture->registry.objects,
                                             span("first"), &value, &present, NULL) ||
                          present;
    event(resources, 5);
}

static bool edges(void) {
    for (size_t mode = 0; mode < 3; ++mode) {
        Fixture fixture;
        CHECK(prepare(&fixture, 0));
        fixture.objects[0].camera_animation = true;
        fixture.objects[0].renderable = 17;
        Resources resources = {
            .fixture = &fixture, .fail_camera = mode == 1, .fail_scene = mode == 2};
        AbGameObjectCoreHost host = {&resources,        camera,  retain,
                                     remove_renderable, release, destroy_object};
        bool removed = false;
        CHECK(ab_game_object_remove_core(&fixture.registry, span("first"), &host,
                                         &removed, NULL) == (mode == 0));
        CHECK(!resources.invalid && !ab_world_has_body(&fixture.world, 0));
        CHECK(removed == (mode == 0));
        if (mode == 0) {
            const uint32_t expected[] = {1, 2, 3, 4, 5, 6};
            CHECK(resources.count == 6 &&
                  !memcmp(resources.events, expected, sizeof(expected)));
        } else {
            CHECK(resources.count == (mode == 1 ? 1u : 4u));
            uintptr_t value = 0;
            bool present = false;
            CHECK(ab_string_map_get(fixture.registry.objects, span("first"), &value,
                                    &present, NULL) &&
                  present);
            CHECK(fixture.objects[0].renderable == 17);
        }
        close_fixture(&fixture);
    }
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Object removal sample %u %s[%zu]: C=%08x original=%08x\n",
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
          !strcmp(line, "AB_ANDROID_GAME_OBJECT_REMOVE_POLICY_V1 256\n"));
    size_t matched = 0, removed_count = 0;
    for (uint32_t sample = 0; sample < 256; ++sample) {
        Fixture fixture;
        CHECK(prepare(&fixture, sample));
        CHECK(words(file, fixture.input, 11, sample, "input", &matched));
        bool removed = false;
        CHECK(ab_game_object_remove_core(
            &fixture.registry, span(names[fixture.input[2]]), NULL, &removed, NULL));
        uint32_t state[13];
        CHECK(snapshot(&fixture, state) &&
              words(file, state, 13, sample, "state", &matched));
        CHECK(removed == (fixture.input[2] < 2));
        CHECK(fixture.objects[0].body == 0 && fixture.objects[1].body == 1);
        removed_count += removed;
        close_fixture(&fixture);
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB game object removal policy complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 256 original native removal prefixes match: %zu words, %zu physical "
           "removals.\n",
           matched, removed_count);
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
