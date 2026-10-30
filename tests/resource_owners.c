#include "ab/resource_owners.h"
#include "ab/asset_groups.h"
#include "sprite_registry_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Resource owner check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    size_t references;
    bool image, bad;
    AbResourceOwners *owners;
} SourceState;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static void retain(const AbResourceSource *source) {
    SourceState *state = source->context;
    state->bad = state->bad || !state->references ||
                 ab_resource_owners_put(state->owners, (AbSpan){0}, source, NULL);
    ++state->references;
}

static void release(const AbResourceSource *source) {
    SourceState *state = source->context;
    state->bad = state->bad || state->references <= 1;
    --state->references;
}

static void unload(const AbResourceSource *source) {
    SourceState *state = source->context;
    state->image = false;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, words + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static bool last_definition(const uint32_t input[24], size_t source, size_t item) {
    if (!(input[source] & (1u << item))) {
        return false;
    }
    for (size_t later = item + 1; later < 3; ++later) {
        if ((input[source] & (1u << later)) &&
            input[4 + source * 3 + item] == input[4 + source * 3 + later]) {
            return false;
        }
    }
    return true;
}

static bool native_case(FILE *file, size_t sample, bool cache) {
    uint32_t input[24], generated[24], tokens[12] = {0};
    registry_case(sample, generated);
    CHECK(read_words(file, input, 24) && memcmp(input, generated, sizeof(input)) == 0);
    AbSpriteRegistryEntry registered[32];
    AbSpriteRegistry registry = {registered, 0, 32};
    AbResourceOwnerEntry entries[8];
    AbResourceOwners owners;
    CHECK(ab_resource_owners_init(&owners, entries, 8, &registry, NULL));
    AbResourceSource sources[4];
    AbResourceMember members[4][3];
    SourceState states[4];
    for (size_t source = 0; source < 4; ++source) {
        states[source] = (SourceState){1, true, false, &owners};
        sources[source] =
            (AbResourceSource){.kind = source < 2 ? AB_SPRITE_REGISTRY_SPRITE
                                                  : AB_SPRITE_REGISTRY_COMPOSITE,
                               .members = members[source],
                               .context = states + source,
                               .retain = retain,
                               .release = release,
                               .unload = source < 2 ? unload : NULL};
        for (size_t item = 0; item < 3; ++item) {
            if (last_definition(input, source, item)) {
                members[source][sources[source].count++] = (AbResourceMember){
                    text(registry_names[input[4 + source * 3 + item]]),
                    tokens + source * 3 + item};
            }
        }
    }
    for (size_t step = 0; step < 8; ++step) {
        uint32_t command = input[16 + step], source = command & 3u;
        AbSpan key = text(registry_owners[(command >> 3) & 3u]);
        if (command & 4u) {
            char derived[256];
            size_t length;
            CHECK(ab_asset_sheet_key(key, derived, sizeof(derived), &length, NULL) &&
                  ab_resource_owners_remove(&owners, sources[source].kind,
                                            (AbSpan){(const uint8_t *)derived, length},
                                            (command & 32u) != 0, NULL));
        } else {
            CHECK(ab_resource_owners_put(&owners, key, sources + source, NULL));
        }
        uint32_t expected[10], actual[10] = {0};
        size_t references[4] = {1, 1, 1, 1};
        CHECK(read_words(file, expected, 10));
        for (size_t owner = 0; owner < 4; ++owner) {
            for (unsigned kind = 1; kind <= 2; ++kind) {
                const AbResourceSource *found = ab_resource_owners_find(
                    &owners, (AbSpriteRegistryKind)kind, text(registry_owners[owner]));
                for (size_t candidate = 0; candidate < 4; ++candidate) {
                    if (found == sources + candidate) {
                        actual[owner * 2 + kind - 1] = (uint32_t)candidate + 1;
                        ++references[candidate];
                    }
                }
            }
        }
        actual[8] = states[0].image;
        actual[9] = states[1].image;
        if (memcmp(actual, expected, sizeof(actual)) != 0) {
            fprintf(stderr, "Original resource owner sample %zu step %zu differs\n",
                    sample, step);
            return false;
        }
        for (size_t index = 0; index < 4; ++index) {
            CHECK(states[index].references == references[index] && !states[index].bad);
        }
        if (cache) {
            uint32_t expected_cache[6], actual_cache[6] = {0};
            CHECK(read_words(file, expected_cache, 6));
            for (size_t owner = 1; owner <= 2; ++owner) {
                char path[64], key_bytes[256];
                int size =
                    snprintf(path, sizeof(path), "missing/%s%s.dat",
                             registry_owners[owner], sample & 1u ? ".extra" : "");
                size_t key_size = 0;
                CHECK(size > 0 && (size_t)size < sizeof(path) &&
                      ab_asset_sheet_key((AbSpan){(const uint8_t *)path, (size_t)size},
                                         key_bytes, sizeof(key_bytes), &key_size,
                                         NULL));
                for (unsigned kind = 1; kind <= 2; ++kind) {
                    const AbResourceSource *selected = ab_resource_owners_find(
                        &owners, (AbSpriteRegistryKind)kind,
                        (AbSpan){(const uint8_t *)key_bytes, key_size});
                    for (size_t source_index = 0; source_index < 4; ++source_index) {
                        if (selected == sources + source_index) {
                            actual_cache[(owner - 1) * 2 + kind - 1] =
                                (uint32_t)source_index + 1;
                        }
                    }
                }
            }
            actual_cache[4] = states[0].image;
            actual_cache[5] = states[1].image;
            CHECK(memcmp(actual_cache, expected_cache, sizeof(actual_cache)) == 0);
        }
        for (size_t name = 0; name < 5; ++name) {
            uint32_t expected_binding[9], binding[9] = {0};
            CHECK(read_words(file, expected_binding, 9));
            for (unsigned kind = 0; kind < 3; ++kind) {
                AbSpriteRegistryEntry found;
                if (!ab_sprite_registry_find(&registry, text(registry_names[name]),
                                             (AbSpriteRegistryKind)kind, &found)) {
                    continue;
                }
                binding[kind * 3] = (uint32_t)found.kind;
                for (uint32_t owner = 0; owner < 4; ++owner) {
                    AbSpan wanted = text(registry_owners[owner]);
                    if (found.owner.size == wanted.size &&
                        (!wanted.size ||
                         memcmp(found.owner.data, wanted.data, wanted.size) == 0)) {
                        binding[kind * 3 + 1] = owner + 1;
                    }
                }
                CHECK(binding[kind * 3 + 1]);
                binding[kind * 3 + 2] =
                    (uint32_t)((const uint32_t *)found.resource - tokens) + 1;
            }
            if (memcmp(binding, expected_binding, sizeof(binding)) != 0) {
                fprintf(
                    stderr,
                    "Original owner registry sample %zu step %zu name %zu differs\n",
                    sample, step, name);
                return false;
            }
        }
    }
    CHECK(ab_resource_owners_clear(&owners, NULL) && !owners.count && !registry.count);
    for (size_t source = 0; source < 4; ++source) {
        CHECK(states[source].references == 1 && !states[source].bad);
    }
    return true;
}

static bool native_checks(FILE *file, bool cache) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, cache ? "AB_ANDROID_RESOURCE_CACHE_V1 4096\n"
                             : "AB_ANDROID_RESOURCE_OWNERS_V1 4096\n") == 0);
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(native_case(file, sample, cache));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, cache ? "AB resource cache reference complete\n"
                             : "AB resource owners reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 32,768 original owner operations: map replacement/removal, retained "
         "sheet image unloading and all typed sprite/composite bindings");
    if (cache) {
        puts("Original no-option sheet/composite cache hits preserve source identity, "
             "image state and every registry binding");
    }
    return true;
}

static bool edge_checks(void) {
    uint32_t tokens[2] = {0};
    AbSpriteRegistryEntry registered[2];
    AbSpriteRegistry registry = {registered, 0, 1};
    AbResourceOwnerEntry entries[1];
    AbResourceOwners owners;
    CHECK(ab_resource_owners_init(&owners, entries, 1, &registry, NULL));
    SourceState state = {1, true, false, &owners};
    AbResourceMember members[] = {{text("bird"), tokens}, {text("pig"), tokens + 1}};
    AbResourceSource source = {
        AB_SPRITE_REGISTRY_SPRITE, members, 1, &state, retain, release, unload};
    char key[] = "first";
    CHECK(ab_resource_owners_put(&owners, text(key), &source, NULL) &&
          state.references == 2 && registry.count == 1);
    key[0] = 'x';
    CHECK(ab_resource_owners_find(&owners, source.kind, text("first")) == &source &&
          !ab_resource_owners_put(&owners, text("second"), &source, NULL) &&
          state.references == 2 && registry.count == 1);
    AbResourceSource larger = source;
    larger.count = 2;
    CHECK(!ab_resource_owners_put(&owners, text("first"), &larger, NULL) &&
          state.references == 2 && registry.count == 1);
    CHECK(ab_resource_owners_remove(&owners, source.kind, text("first"), true, NULL) &&
          owners.count == 1 && !registry.count && !state.image &&
          state.references == 2);
    CHECK(ab_resource_owners_put(&owners, text("first"), &source, NULL) &&
          registry.count == 1 && state.references == 2 && !state.image);
    CHECK(ab_resource_owners_clear(&owners, NULL) && !owners.count && !registry.count &&
          state.references == 1 && !state.bad);
    CHECK(
        ab_resource_owners_remove(&owners, source.kind, text("missing"), false, NULL));
    CHECK(!ab_resource_owners_put(&owners, (AbSpan){(const uint8_t *)"x", SIZE_MAX},
                                  &source, NULL));
    CHECK(!ab_resource_owners_init(NULL, entries, 1, &registry, NULL));
    CHECK(!ab_resource_owners_init(&owners, entries, SIZE_MAX, &registry, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    bool cache = argc == 3 && strcmp(argv[1], "--cache") == 0;
    if (argc != 2 && !cache) {
        return 2;
    }
    FILE *file = fopen(argv[cache ? 2 : 1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file, cache);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
