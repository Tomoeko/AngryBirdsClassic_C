#include "ab/sprite_resource.h"
#include "ab/asset_groups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Sprite resource check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool same(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool edge_checks(void) {
    const uint8_t zip_bytes[22] = {'P', 'K', 5, 6};
    AbZip archive = {0};
    AbSpriteResource *resource = NULL;
    CHECK(ab_zip_open((AbSpan){zip_bytes, sizeof(zip_bytes)}, &archive, NULL));
    CHECK(!ab_sprite_resource_open(&archive, "missing", 1024, 1024, &resource, NULL) &&
          !resource);
    CHECK(!ab_sprite_resource_open(&archive, "missing", 1, 1024, &resource, NULL) &&
          !resource);
    CHECK(!ab_sprite_resource_open(NULL, "missing", 1024, 1024, &resource, NULL) &&
          !resource && !ab_sprite_resource_source(NULL) &&
          !ab_sprite_resource_atlas(NULL) && !ab_sprite_resource_bytes(NULL) &&
          !ab_sprite_resource_path(NULL).size && !ab_sprite_set_bytes(NULL));
    AbSpriteAtlasBinding output = {.image_index = 99};
    CHECK(!ab_sprite_resource_find(NULL, NULL, text("missing"), &output) &&
          output.image_index == 99);
    ab_sprite_resource_close(NULL);
    ab_zip_close(&archive);
    return true;
}

static bool resource_checks(const AbZip *archive) {
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat"};
    AbSpriteResource *first = NULL, *second = NULL;
    AbError error = {0};
    CHECK(ab_sprite_resource_open(archive, paths[0], 32 * 1024 * 1024, 16 * 1024 * 1024,
                                  &first, &error));
    CHECK(ab_sprite_resource_open(archive, paths[1], 32 * 1024 * 1024, 16 * 1024 * 1024,
                                  &second, &error));
    const AbResourceSource *first_source = ab_sprite_resource_source(first);
    const AbResourceSource *second_source = ab_sprite_resource_source(second);
    CHECK(first_source && first_source->kind == AB_SPRITE_REGISTRY_SPRITE &&
          first_source->count && first_source->count <= 4096 && second_source &&
          second_source->count && second_source->count <= 4096 &&
          same(ab_sprite_resource_path(first), text(paths[0])) &&
          ab_sprite_atlas_count(ab_sprite_resource_atlas(first)) == 1 &&
          ab_sprite_atlas_count(ab_sprite_resource_atlas(second)) == 1);
    AbSpriteRegistryEntry *bindings = calloc(4096, sizeof(*bindings));
    CHECK(bindings);
    AbSpriteRegistry registry = {bindings, 0, 4096};
    AbResourceOwnerEntry entries[2];
    AbResourceOwners owners;
    CHECK(ab_resource_owners_init(&owners, entries, 2, &registry, NULL));
    char key[256];
    size_t key_length;
    CHECK(ab_asset_sheet_key(text(paths[0]), key, sizeof(key), &key_length, NULL));
    AbSpan owner_key = {(const uint8_t *)key, key_length};
    CHECK(ab_resource_owners_put(&owners, owner_key, first_source, NULL) &&
          registry.count == first_source->count);
    AbSpan first_name = first_source->members[0].name;
    AbSpriteAtlasBinding image;
    CHECK(ab_sprite_resource_find(first, &registry, first_name, &image) &&
          image.sprite.region == first_source->members[0].resource && image.image &&
          image.image->width && image.image->height && image.image->rgba.size);
    size_t first_bytes = ab_sprite_resource_bytes(first);
    size_t first_members = first_source->count;
    ab_sprite_resource_close(first);
    /* Only the map reference now keeps this borrowed pointer alive. */
    CHECK(ab_resource_owners_remove(&owners, AB_SPRITE_REGISTRY_SPRITE, owner_key, true,
                                    NULL) &&
          !registry.count && owners.count == 1 &&
          ab_resource_owners_find(&owners, AB_SPRITE_REGISTRY_SPRITE, owner_key) ==
              first_source &&
          !ab_sprite_resource_atlas(first) && first_source->count == first_members &&
          ab_sprite_resource_bytes(first) < first_bytes);
    AbSpriteAtlasBinding missing = {.image_index = 99};
    CHECK(!ab_sprite_resource_find(first, &registry, first_name, &missing) &&
          missing.image_index == 99);
    CHECK(ab_resource_owners_put(&owners, owner_key, second_source, NULL) &&
          registry.count == second_source->count);
    /* Replacement released the final first-source reference. */
    first = NULL;
    size_t second_bytes = ab_sprite_resource_bytes(second);
    size_t second_members = second_source->count;
    CHECK(ab_sprite_resource_find(second, &registry, second_source->members[0].name,
                                  &image) &&
          image.sprite.region == second_source->members[0].resource);
    ab_sprite_resource_close(second);
    CHECK(ab_resource_owners_clear(&owners, NULL) && !owners.count && !registry.count);
    second = NULL;
    free(bindings);
    printf("Decoded real sprite sources with %zu/%zu final named members and "
           "%zu/%zu retained bytes; map references survive caller release, image "
           "unload preserves metadata, and replacement/teardown release ownership.\n",
           first_members, second_members, first_bytes, second_bytes);
    CHECK(!ab_sprite_resource_open(archive, paths[0], 32 * 1024 * 1024, 0, &first,
                                   NULL) &&
          !first);
    CHECK(!ab_sprite_resource_open(archive, paths[0], 1, 16 * 1024 * 1024, &first,
                                   NULL) &&
          !first);
    return true;
}

static bool apk_checks(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(path, 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                resource_checks(&archive);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    return apk_checks(argv[2]) ? 0 : 1;
}
