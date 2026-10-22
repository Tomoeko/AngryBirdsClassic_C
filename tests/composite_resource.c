#include "ab/composite_resources.h"
#include "ab/composite_resource.h"
#include "ab/composite_part.h"
#include "ab/sprite_resource.h"
#include "ab/asset_groups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Composite resource check failed at %d: %s\n", __LINE__,   \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbResourceOwners *owners;
    size_t calls;
} Resolver;

static bool resolve(void *context, AbSpan name, AbSpriteBinding *output,
                    AbError *error) {
    Resolver *resolver = context;
    AbResourceMemberView selected;
    ++resolver->calls;
    if (!ab_resource_owners_find_member(resolver->owners, AB_SPRITE_REGISTRY_SPRITE,
                                        name, &selected)) {
        return ab_fail(error, "In-game composite sprite is absent: %.*s",
                       (int)name.size, (const char *)name.data);
    }
    const AbSpriteResource *source = ab_sprite_resource_from_source(selected.source);
    return ab_sprite_resource_member(source, selected.member, output) ||
           ab_fail(error, "Composite source is not a decoded SPRT provider");
}

static bool edge_checks(void) {
    const uint8_t empty_zip[22] = {'P', 'K', 5, 6};
    AbZip archive = {0};
    AbResourceOwners sheets = {0};
    AbCompositeResource *resource = NULL;
    CHECK(ab_zip_open((AbSpan){empty_zip, sizeof(empty_zip)}, &archive, NULL));
    CHECK(!ab_composite_resource_open(&archive, "missing", &sheets, 4096, 1024,
                                      &resource, NULL) &&
          !resource);
    CHECK(!ab_composite_resource_open(NULL, "missing", &sheets, 4096, 1024, &resource,
                                      NULL) &&
          !resource && !ab_composite_resource_source(NULL) &&
          !ab_composite_resource_from_source(NULL) &&
          !ab_composite_resource_bytes(NULL));
    AbResolvedComposite view = {.extended = true};
    CHECK(!ab_composite_resource_find(NULL, NULL, (AbSpan){0}, &view) && view.extended);
    ab_composite_resource_close(NULL);
    ab_zip_close(&archive);
    return true;
}

static bool owner_checks(const AbZip *archive, const char *path,
                         AbResourceOwners *owners) {
    AbCompositeResource *first = NULL, *second = NULL;
    CHECK(ab_composite_resource_open(archive, path, owners, 1024 * 1024, 1024 * 1024,
                                     &first, NULL));
    const AbResourceSource *source = ab_composite_resource_source(first);
    CHECK(source && source->kind == AB_SPRITE_REGISTRY_COMPOSITE &&
          source->count == 35 && ab_composite_resource_from_source(source) == first);
    size_t bytes = ab_composite_resource_bytes(first);
    CHECK(ab_composite_resource_open(archive, path, owners, bytes, 1024 * 1024, &second,
                                     NULL));
    ab_composite_resource_close(second);
    second = NULL;
    CHECK(!ab_composite_resource_open(archive, path, owners, bytes - 1, 1024 * 1024,
                                      &second, NULL) &&
          !second);
    CHECK(!ab_composite_resource_open(archive, path, owners, bytes, 0, &second, NULL) &&
          !second);
    AbSpan key = {(const uint8_t *)"composites", 10};
    CHECK(ab_resource_owners_put(owners, key, source, NULL));
    AbResolvedComposite view;
    for (size_t index = 0; index < source->count; ++index) {
        CHECK(ab_composite_resource_find(first, owners->registry,
                                         source->members[index].name, &view) &&
              source->members[index].resource && view.source && view.extended);
    }
    AbSpan name = source->members[0].name;
    /* Ordinary bindings and foreign composites mask an older source. */
    int foreign = 0;
    CHECK(ab_sprite_registry_add(
        owners->registry,
        (AbSpriteRegistryEntry){name, key, AB_SPRITE_REGISTRY_SPRITE, &foreign}, NULL));
    CHECK(!ab_composite_resource_find(first, owners->registry, name, &view));
    CHECK(ab_sprite_registry_remove(owners->registry, name, AB_SPRITE_REGISTRY_SPRITE,
                                    &foreign, NULL));
    CHECK(ab_sprite_registry_add(
        owners->registry,
        (AbSpriteRegistryEntry){name, key, AB_SPRITE_REGISTRY_COMPOSITE, &foreign},
        NULL));
    CHECK(!ab_composite_resource_find(first, owners->registry, name, &view));
    CHECK(ab_sprite_registry_remove(owners->registry, name,
                                    AB_SPRITE_REGISTRY_COMPOSITE, &foreign, NULL));
    ab_composite_resource_close(first);
    CHECK(ab_composite_resource_find(first, owners->registry, name, &view));
    CHECK(ab_composite_resource_open(archive, path, owners, 1024 * 1024, 1024 * 1024,
                                     &second, NULL));
    CHECK(ab_resource_owners_put(owners, key, ab_composite_resource_source(second),
                                 NULL));
    first = NULL; /* Replacement releases the previous map's final reference. */
    ab_composite_resource_close(second);
    size_t members = ab_composite_resource_source(second)->count;
    /* Release sheet sources first, as the authored load list does. The weak
     * parts must not be inspected after this point; composite teardown is safe. */
    for (size_t index = 0; index < owners->capacity; ++index) {
        AbResourceOwnerEntry *entry = owners->entries + index;
        if (entry->used && entry->source->kind == AB_SPRITE_REGISTRY_SPRITE) {
            CHECK(ab_resource_owners_remove(owners, AB_SPRITE_REGISTRY_SPRITE,
                                            (AbSpan){entry->key, entry->key_size},
                                            false, NULL));
        }
    }
    CHECK(owners->count == 1 && owners->registry->count == members);
    CHECK(ab_resource_owners_remove(owners, AB_SPRITE_REGISTRY_COMPOSITE, key, true,
                                    NULL) &&
          !owners->count && !owners->registry->count);
    second = NULL;
    printf("Composite source ownership retains %zu bytes and %zu members; "
           "replacement, masking and teardown after weak sheet release pass.\n",
           bytes, members);
    return true;
}

static bool resource_checks(const AbZip *archive) {
    const char *names[] = {"INGAME_BLOCKS_1",       "INGAME_BLOCKS_2",
                           "INGAME_BLOCKS_3",       "INGAME_EDITOR_GROUND_BLOCKS",
                           "INGAME_EXPLOSIONS_1",   "INGAME_EXPLOSIONS_2",
                           "INGAME_TRANSPARENCY_1", "INGAME_TRANSPARENCY_2"};
    AbSpriteRegistryEntry *registered = calloc(8192, sizeof(*registered));
    CHECK(registered);
    AbSpriteRegistry registry = {registered, 0, 8192};
    AbResourceOwnerEntry entries[9];
    AbResourceOwners owners;
    CHECK(ab_resource_owners_init(&owners, entries, 9, &registry, NULL));
    size_t sprite_bytes = 0;
    AbError error = {0};
    for (size_t index = 0; index < 8; ++index) {
        char path[256], key[256];
        int length = snprintf(path, sizeof(path), "assets/data/images/base/%s.dat",
                              names[index]);
        CHECK(length > 0 && (size_t)length < sizeof(path));
        AbSpriteResource *resource = NULL;
        if (!ab_sprite_resource_open(archive, path, 32 * 1024 * 1024, 16 * 1024 * 1024,
                                     &resource, &error)) {
            fprintf(stderr, "%s\n", error.message);
            return false;
        }
        size_t key_length;
        CHECK(ab_asset_sheet_key((AbSpan){(const uint8_t *)path, (size_t)length}, key,
                                 sizeof(key), &key_length, NULL) &&
              ab_resource_owners_put(&owners,
                                     (AbSpan){(const uint8_t *)key, key_length},
                                     ab_sprite_resource_source(resource), NULL));
        sprite_bytes += ab_sprite_resource_bytes(resource);
        CHECK(sprite_bytes < 64 * 1024 * 1024);
        ab_sprite_resource_close(resource);
    }
    const char *path = "assets/data/images/base/INGAME_COMPOSPRITES.dat";
    AbCompositeResources *composites = NULL;
    Resolver resolver = {&owners, 0};
    if (!ab_composite_resources_open_resolved(archive, &path, 1, 4 * 1024 * 1024,
                                              1024 * 1024, resolve, &resolver,
                                              &composites, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return false;
    }
    size_t count = ab_composite_resources_count(composites), parts = 0;
    CHECK(count);
    for (size_t index = 0; index < count; ++index) {
        AbResolvedComposite composite;
        CHECK(ab_composite_resources_item(composites, index, &composite) &&
              composite.extended);
        size_t number = composite.source->part_count;
        AbCompositeBoundsPart *states = number ? calloc(number, sizeof(*states)) : NULL;
        CHECK(!number || states);
        for (size_t part = 0; part < number; ++part) {
            CHECK(ab_composite_part_state(composite.source->parts + part, true,
                                          composite.parts[part].region, states + part,
                                          NULL));
            char name[512];
            size_t length;
            CHECK(ab_composite_part_name(composite.source->parts + part, true, name,
                                         sizeof(name), &length, NULL));
        }
        AbCompositeBounds bounds;
        CHECK(ab_composite_bounds(states, number, &bounds, NULL) &&
              memcmp(&bounds, &composite.bounds, sizeof(bounds)) == 0);
        free(states);
        parts += number;
    }
    CHECK(resolver.calls == parts);
    printf("Resolved %zu real in-game RVIO composites and %zu part snapshots using "
           "eight decoded sheet sources (%zu sprite bytes, %zu composite bytes).\n",
           count, parts, sprite_bytes, ab_composite_resources_bytes(composites));
    ab_composite_resources_close(composites);
    CHECK(owner_checks(archive, path, &owners));
    CHECK(ab_resource_owners_clear(&owners, NULL) && !registry.count);
    free(registered);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                resource_checks(&archive);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
