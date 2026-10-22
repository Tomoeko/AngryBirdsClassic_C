#include "ab/sprite_registry.h"
#include "sprite_registry_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Sprite registry check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, words + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static bool final_definition(const uint32_t input[24], size_t set, size_t item) {
    if (!(input[set] & (1u << item))) {
        return false;
    }
    for (size_t later = item + 1; later < 3; ++later) {
        if ((input[set] & (1u << later)) &&
            input[4 + set * 3 + item] == input[4 + set * 3 + later]) {
            return false;
        }
    }
    return true;
}

static bool native_case(FILE *file, size_t sample) {
    uint32_t input[24], expected_input[24], tokens[12] = {0};
    registry_case(sample, expected_input);
    CHECK(read_words(file, input, 24) &&
          memcmp(input, expected_input, sizeof(input)) == 0);
    AbSpriteRegistryEntry entries[32];
    AbSpriteRegistry registry = {entries, 0, 32};
    for (size_t step = 0; step < 8; ++step) {
        uint32_t command = input[16 + step];
        size_t set = command & 3u;
        AbSpriteRegistryKind type =
            set < 2 ? AB_SPRITE_REGISTRY_SPRITE : AB_SPRITE_REGISTRY_COMPOSITE;
        for (size_t item = 0; item < 3; ++item) {
            if (!final_definition(input, set, item)) {
                continue;
            }
            AbSpan name = text(registry_names[input[4 + set * 3 + item]]);
            const void *payload = tokens + set * 3 + item;
            if (command & 4u) {
                CHECK(ab_sprite_registry_remove(&registry, name, type, payload, NULL));
            } else {
                AbSpriteRegistryEntry entry = {
                    .name = name,
                    .owner = text(registry_owners[(command >> 3) & 3u]),
                    .kind = type,
                    .resource = payload};
                CHECK(ab_sprite_registry_add(&registry, entry, NULL));
            }
        }
        for (size_t name = 0; name < 5; ++name) {
            uint32_t expected[9], actual[9] = {0};
            CHECK(read_words(file, expected, 9));
            for (unsigned kind = 0; kind < 3; ++kind) {
                AbSpriteRegistryEntry found;
                if (!ab_sprite_registry_find(&registry, text(registry_names[name]),
                                             (AbSpriteRegistryKind)kind, &found)) {
                    continue;
                }
                actual[kind * 3] = (uint32_t)found.kind;
                bool owner_found = false;
                for (size_t owner = 0; owner < 4; ++owner) {
                    if (found.owner.size == strlen(registry_owners[owner]) &&
                        (!found.owner.size ||
                         memcmp(found.owner.data, registry_owners[owner],
                                found.owner.size) == 0)) {
                        actual[kind * 3 + 1] = (uint32_t)owner + 1;
                        owner_found = true;
                    }
                }
                CHECK(owner_found);
                actual[kind * 3 + 2] =
                    (uint32_t)((const uint32_t *)found.resource - tokens) + 1;
            }
            for (size_t field = 0; field < 9; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Native registry sample %zu step %zu name %zu field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            sample, step, name, field, actual[field], expected[field]);
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
          strcmp(line, "AB_ANDROID_SPRITE_REGISTRY_V1 4096\n") == 0);
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(native_case(file, sample));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB sprite registry reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 32,768 original sprite registry commands and 491,520 typed/name "
         "queries, including mixed sprite/composite shadowing and repeated removal");
    return true;
}

static bool edge_checks(void) {
    uint32_t tokens[2] = {0};
    AbSpriteRegistryEntry entries[4], found = {.resource = tokens + 1};
    AbSpriteRegistry registry = {entries, 0, 4};
    AbSpriteRegistryEntry first = {text("bird"), text("base"),
                                   AB_SPRITE_REGISTRY_SPRITE, tokens};
    CHECK(ab_sprite_registry_add(&registry, first, NULL));
    AbSpriteRegistryEntry second = {text("bird"), text("other"),
                                    AB_SPRITE_REGISTRY_COMPOSITE, tokens + 1};
    CHECK(ab_sprite_registry_add(&registry, second, NULL));
    CHECK(!ab_sprite_registry_find(&registry, text("bird"), AB_SPRITE_REGISTRY_SPRITE,
                                   &found) &&
          found.resource == tokens + 1);
    CHECK(ab_sprite_registry_remove(&registry, text("bird"), second.kind,
                                    second.resource, NULL));
    first.owner = text("alternate");
    CHECK(ab_sprite_registry_add(&registry, first, NULL));
    AbSpriteRegistryEntry alias = first;
    alias.name = (AbSpan){0};
    CHECK(ab_sprite_registry_add(&registry, alias, NULL));
    CHECK(ab_sprite_registry_remove(&registry, text("bird"), first.kind, first.resource,
                                    NULL) &&
          registry.count == 1 &&
          ab_sprite_registry_find(&registry, (AbSpan){0}, AB_SPRITE_REGISTRY_ANY,
                                  &found) &&
          found.resource == tokens);
    CHECK(ab_sprite_registry_remove(&registry, text("bird"), first.kind, first.resource,
                                    NULL) &&
          registry.count == 1);
    first.name = (AbSpan){NULL, 1};
    CHECK(!ab_sprite_registry_add(&registry, first, NULL) && registry.count == 1);
    CHECK(!ab_sprite_registry_remove(&registry, alias.name, AB_SPRITE_REGISTRY_ANY,
                                     tokens, NULL) &&
          registry.count == 1);
    registry.capacity = 1;
    CHECK(!ab_sprite_registry_add(&registry, alias, NULL) && registry.count == 1);
    registry.capacity = 0;
    CHECK(!ab_sprite_registry_remove(&registry, alias.name, alias.kind, tokens, NULL) &&
          registry.count == 1);
    AbSpriteRegistry empty = {0};
    CHECK(ab_sprite_registry_remove(&empty, alias.name, alias.kind, tokens, NULL) &&
          !ab_sprite_registry_add(&empty, alias, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
