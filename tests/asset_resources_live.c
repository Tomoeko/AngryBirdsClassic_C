#include "ab/asset_resources.h"
#include "ab/android_assets.h"
#include "ab/composite_part.h"
#include "ab/draw.h"
#include "ab/target_resources.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Live asset resource check failed at %d: %s\n", __LINE__,  \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbSpan input;
    size_t offset;
} Reader;

typedef struct {
    Reader reader;
    const AbZip *archive;
    AbAndroidAssets *assets;
    AbAssetResources *resources;
    AbDataEntry groups[64];
    size_t sprites, composites, parts, bindings;
} Comparison;

static bool read_words(Reader *reader, uint32_t *output, size_t count) {
    CHECK(count <= SIZE_MAX / 4 &&
          ab_range(reader->input.size, reader->offset, count * 4));
    for (size_t index = 0; index < count; ++index) {
        output[index] = ab_u32le(reader->input.data + reader->offset + index * 4);
    }
    reader->offset += count * 4;
    return true;
}

static bool read_text(Reader *reader, AbSpan *output) {
    uint32_t size;
    CHECK(read_words(reader, &size, 1) && size <= 1024 &&
          ab_slice(reader->input, reader->offset, size, output));
    reader->offset += size;
    return true;
}

static bool same(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static const AbResourceMember *member(const AbResourceSource *source, AbSpan name) {
    for (size_t index = 0; source && index < source->count; ++index) {
        if (same(source->members[index].name, name)) {
            return source->members + index;
        }
    }
    return NULL;
}

static bool compare_sprite(Comparison *comparison, const AbResourceSource *source,
                           const AbResourceMember *selected) {
    uint32_t expected[12], actual[12];
    CHECK(read_words(&comparison->reader, expected, 12));
    const AbSpriteResource *resource = ab_sprite_resource_from_source(source);
    AbSpriteBinding binding;
    AbSpriteAtlasImage image;
    CHECK(ab_sprite_resource_member(resource, selected, &binding) &&
          ab_sprite_atlas_image(ab_sprite_resource_atlas(resource), 0, &image));
    const AbSpriteRegion *region = binding.region;
    actual[0] = (uint16_t)region->x | ((uint32_t)(uint16_t)region->y << 16);
    actual[1] = (uint16_t)region->width | ((uint32_t)(uint16_t)region->height << 16);
    actual[2] = bits((float)region->pivot_x);
    actual[3] = bits((float)region->pivot_y);
    AbDrawQuad quad;
    CHECK(ab_sprite_quad(region, image.image->width, image.image->height,
                         (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad, NULL));
    for (size_t index = 0; index < 4; ++index) {
        actual[4 + index * 2] = bits(quad.vertices[index].u);
        actual[5 + index * 2] = bits(quad.vertices[index].v);
    }
    if (memcmp(expected, actual, sizeof(actual)) != 0) {
        fprintf(stderr, "Original source sprite differs: %.*s\n",
                (int)selected->name.size, (const char *)selected->name.data);
        return false;
    }
    ++comparison->sprites;
    return true;
}

static bool compare_part(Comparison *comparison, const AbResolvedComposite *composite,
                         size_t index) {
    AbSpan expected_name, source_key, sprite_name;
    uint32_t expected[8];
    CHECK(read_text(&comparison->reader, &expected_name) &&
          read_text(&comparison->reader, &source_key) &&
          read_text(&comparison->reader, &sprite_name) &&
          read_words(&comparison->reader, expected, 8));
    const AbResourceSource *sheet =
        ab_resource_owners_find(ab_asset_resources_owners(comparison->resources),
                                AB_SPRITE_REGISTRY_SPRITE, source_key);
    const AbResourceMember *sprite = member(sheet, sprite_name);
    const AbCompositePart *part = composite->source->parts + index;
    char name[1025];
    size_t name_size;
    AbCompositeBoundsPart state;
    CHECK(sprite && sprite->resource == composite->parts[index].region &&
          same(sprite_name, part->sprite) &&
          ab_composite_part_name(part, composite->extended, name, sizeof(name),
                                 &name_size, NULL) &&
          same(expected_name, (AbSpan){(const uint8_t *)name, name_size}) &&
          ab_composite_part_state(part, composite->extended,
                                  composite->parts[index].region, &state, NULL));
    uint32_t actual[8] = {bits(state.offset.x),  bits(state.offset.y),
                          bits(state.scale.x),   bits(state.scale.y),
                          bits(state.factors.x), bits(state.factors.y),
                          bits(state.rotation),  state.enabled};
    CHECK(memcmp(expected, actual, sizeof(actual)) == 0);
    ++comparison->parts;
    return true;
}

static bool compare_composite(Comparison *comparison, const AbResourceSource *source,
                              const AbResourceMember *selected) {
    uint32_t expected[5];
    CHECK(read_words(&comparison->reader, expected, 5));
    AbResolvedComposite composite;
    CHECK(ab_composite_resource_member(ab_composite_resource_from_source(source),
                                       selected, &composite));
    const AbCompositeBounds *bounds = &composite.bounds;
    uint32_t actual[5] = {(uint32_t)bounds->width, (uint32_t)bounds->height,
                          (uint32_t)bounds->pivot_x, (uint32_t)bounds->pivot_y,
                          (uint32_t)composite.source->part_count};
    CHECK(memcmp(expected, actual, sizeof(actual)) == 0);
    for (size_t index = 0; index < composite.source->part_count; ++index) {
        CHECK(compare_part(comparison, &composite, index));
    }
    ++comparison->composites;
    return true;
}

static bool compare_sources(Comparison *comparison, size_t count) {
    const AbResourceOwners *owners = ab_asset_resources_owners(comparison->resources);
    CHECK(owners->count == count);
    for (size_t index = 0; index < count; ++index) {
        uint32_t kind, values[2];
        AbSpan key;
        CHECK(read_words(&comparison->reader, &kind, 1) && (kind == 1 || kind == 2) &&
              read_text(&comparison->reader, &key) &&
              read_words(&comparison->reader, values, 2));
        const AbResourceSource *source =
            ab_resource_owners_find(owners, (AbSpriteRegistryKind)kind, key);
        CHECK(source && source->count == values[1]);
        bool image = ab_sprite_resource_atlas(ab_sprite_resource_from_source(source));
        CHECK(image == (values[0] != 0));
        for (size_t item = 0; item < source->count; ++item) {
            AbSpan name;
            CHECK(read_text(&comparison->reader, &name));
            const AbResourceMember *selected = member(source, name);
            CHECK(selected);
            CHECK(kind == 1 ? compare_sprite(comparison, source, selected)
                            : compare_composite(comparison, source, selected));
        }
    }
    return true;
}

static bool compare_registry(Comparison *comparison, size_t count) {
    const AbResourceOwners *owners = ab_asset_resources_owners(comparison->resources);
    const AbSpriteRegistry *registry =
        ab_asset_resources_registry(comparison->resources);
    for (size_t item = 0; item < count; ++item) {
        AbSpan name;
        uint32_t bindings;
        CHECK(read_text(&comparison->reader, &name) &&
              read_words(&comparison->reader, &bindings, 1) && bindings <= 128);
        size_t cursor = 0;
        AbSpriteRegistryEntry last = {0};
        for (size_t index = 0; index < bindings; ++index) {
            uint32_t kind;
            AbSpan owner, source_key, member_name;
            CHECK(read_words(&comparison->reader, &kind, 1) &&
                  read_text(&comparison->reader, &owner) &&
                  read_text(&comparison->reader, &source_key) &&
                  read_text(&comparison->reader, &member_name));
            const AbResourceSource *source =
                ab_resource_owners_find(owners, (AbSpriteRegistryKind)kind, source_key);
            const AbResourceMember *selected = member(source, member_name);
            CHECK(selected);
            while (cursor < registry->count &&
                   !same(registry->entries[cursor].name, name)) {
                ++cursor;
            }
            CHECK(cursor < registry->count);
            last = registry->entries[cursor++];
            CHECK((uint32_t)last.kind == kind && same(last.owner, owner) &&
                  last.resource == selected->resource);
            ++comparison->bindings;
        }
        for (; cursor < registry->count; ++cursor) {
            CHECK(!same(registry->entries[cursor].name, name));
        }
        for (unsigned kind = 0; kind < 3; ++kind) {
            AbSpriteRegistryEntry active;
            bool found = ab_sprite_registry_find(registry, name,
                                                 (AbSpriteRegistryKind)kind, &active);
            bool wanted = bindings && (!kind || kind == (unsigned)last.kind);
            CHECK(found == wanted);
            if (found) {
                CHECK(active.resource == last.resource && active.kind == last.kind &&
                      same(active.owner, last.owner));
            }
        }
    }
    CHECK(comparison->bindings == registry->count);
    return true;
}

static bool compare_state(Comparison *comparison, AbSpan key) {
    Reader *reader = &comparison->reader;
    CHECK(reader->input.size >= 8 && memcmp(reader->input.data, "ABRESL1\0", 8) == 0);
    reader->offset = 8;
    uint32_t header[6];
    CHECK(read_words(reader, header, 6) && !header[2] && header[3] <= 64 &&
          header[4] <= 128 && header[5] <= 8192);
    float width, height;
    memcpy(&width, header, sizeof(width));
    memcpy(&height, header + 1, sizeof(height));
    for (uint32_t index = 0; index < header[3]; ++index) {
        AbSpan name;
        CHECK(read_text(reader, &name));
        comparison->groups[index] =
            (AbDataEntry){{.kind = AB_DATA_NUMBER, .bits = bits((float)(index + 1))},
                          {.kind = AB_DATA_STRING, .string = name}};
    }
    AbDataTable requested = {comparison->groups, header[3], 64, NULL};
    AbAndroidAssetLimits planning = {width, height, false, false,
                                     false, 64,     8,     1024 * 1024};
    AbAssetResourceLimits limits = {128, 8192, 128 * 1024 * 1024, 16 * 1024 * 1024};
    AbError error = {0};
    CHECK(ab_android_assets_open(comparison->archive, key, planning,
                                 &comparison->assets, &error) &&
          ab_asset_resources_open(comparison->archive, limits, &comparison->resources,
                                  &error));
    if (!ab_android_assets_manage(comparison->assets, &requested, true,
                                  ab_asset_resources_host, comparison->resources,
                                  &error)) {
        fprintf(stderr, "%s\n", error.message);
        return false;
    }
    CHECK(ab_android_assets_group_count(comparison->assets) == header[3]);
    CHECK(compare_sources(comparison, header[4]) &&
          compare_registry(comparison, header[5]) &&
          ab_range(reader->input.size, reader->offset, 4) &&
          memcmp(reader->input.data + reader->offset, "END!", 4) == 0 &&
          reader->offset + 4 == reader->input.size);
    printf("Matched original live Android resources: %u groups, %u source maps, "
           "%zu ordinary regions/UVs, %zu composites, %zu part snapshots, "
           "%zu bindings and %u name/type query sets (%zu retained bytes).\n",
           header[3], header[4], comparison->sprites, comparison->composites,
           comparison->parts, comparison->bindings, header[5],
           ab_asset_resources_bytes(comparison->resources));
    return true;
}

static bool compare_images(const Comparison *comparison, AbSpan reference) {
    Reader reader = {.input = reference};
    CHECK(reference.size >= 12 && memcmp(reference.data, "ABRESI1\0", 8) == 0);
    reader.offset = 8;
    uint32_t count;
    CHECK(read_words(&reader, &count, 1) && count == 22);
    const AbResourceOwners *owners = ab_asset_resources_owners(comparison->resources);
    size_t alpha_images = 0;
    for (uint32_t index = 0; index < count; ++index) {
        AbSpan name, path;
        uint32_t expected[6];
        CHECK(read_text(&reader, &name) && read_text(&reader, &path) &&
              read_words(&reader, expected, 6));
        const AbResourceSource *source =
            ab_resource_owners_find(owners, AB_SPRITE_REGISTRY_SPRITE, name);
        const AbSpriteResource *resource = ab_sprite_resource_from_source(source);
        const AbSpriteAtlas *atlas = ab_sprite_resource_atlas(resource);
        AbSpriteAtlasImage image;
        CHECK(resource && ab_sprite_atlas_count(atlas) == 1 &&
              ab_sprite_atlas_image(atlas, 0, &image));
        CHECK(image.image->width == expected[0] && image.image->height == expected[1] &&
              expected[0] == expected[2] && expected[1] == expected[3] &&
              expected[5] == 0 && image.path.size == path.size + 7 &&
              memcmp(image.path.data, "assets/", 7) == 0 &&
              memcmp(image.path.data + 7, path.data, path.size) == 0);
        CHECK(expected[4] == 6 || expected[4] == 18 || expected[4] == 33);
        AbSpriteAlpha alpha =
            expected[4] == 33 ? AB_SPRITE_ALPHA_OPAQUE : AB_SPRITE_ALPHA_PRESENT;
        if (image.alpha != alpha) {
            fprintf(stderr, "Original image alpha format differs: %.*s\n",
                    (int)name.size, (const char *)name.data);
            return false;
        }
        alpha_images += alpha == AB_SPRITE_ALPHA_PRESENT;
    }
    CHECK(ab_range(reference.size, reader.offset, 4) &&
          reader.offset + 4 == reference.size &&
          memcmp(reference.data + reader.offset, "END!", 4) == 0 && alpha_images == 20);
    printf("Matched 22 original live image names/dimensions and alpha formats "
           "(20 alpha, 2 opaque).\n");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        return 2;
    }
    AbBuffer apk = {0}, reference = {0}, images = {0};
    AbZip archive = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    Comparison comparison = {.archive = &archive};
    bool okay = ab_file_read(argv[1], 128 * 1024 * 1024, &apk, &error) &&
                ab_file_read(argv[2], 4 * 1024 * 1024, &reference, &error) &&
                ab_zip_open((AbSpan){apk.data, apk.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error);
    if (okay) {
        comparison.reader.input = (AbSpan){reference.data, reference.size};
        okay = compare_state(&comparison, (AbSpan){key, sizeof(key)});
    }
    if (okay && argc == 4) {
        okay = ab_file_read(argv[3], 1024 * 1024, &images, &error) &&
               compare_images(&comparison, (AbSpan){images.data, images.size});
    }
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_asset_resources_close(comparison.resources);
    ab_android_assets_close(comparison.assets);
    memset(key, 0, sizeof(key));
    ab_zip_close(&archive);
    ab_buffer_free(&reference);
    ab_buffer_free(&images);
    ab_buffer_free(&apk);
    return okay ? 0 : 1;
}
