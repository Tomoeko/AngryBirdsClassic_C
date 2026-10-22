#include "ab/sprite_atlas.h"
#include "ab/draw.h"
#include "ab/hash.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Sprite atlas failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan string(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool same(AbSpan span, const char *text) {
    return span.size == strlen(text) && memcmp(span.data, text, span.size) == 0;
}

static void put16be(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void put32(uint8_t *bytes, uint32_t value, bool big_endian) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(value >> (8 * (big_endian ? 3 - index : index)));
    }
}

typedef struct {
    uint8_t bytes[2048];
    AbZipEntry entries[8];
    AbZip archive;
} Fixture;

/* These fixtures exercise resource ownership with stored payloads. The native
 * comparison below opens the complete pinned APK through the ZIP reader. */
static bool add_entry(Fixture *fixture, const char *path, AbSpan payload) {
    CHECK(fixture->archive.count < 8 &&
          payload.size <= sizeof(fixture->bytes) - fixture->archive.input.size);
    size_t offset = fixture->archive.input.size;
    memcpy(fixture->bytes + offset, payload.data, payload.size);
    fixture->entries[fixture->archive.count++] =
        (AbZipEntry){.name = string(path),
                     .size = (uint32_t)payload.size,
                     .compressed_size = (uint32_t)payload.size,
                     .data_offset = offset,
                     .crc32 = ab_crc32(payload)};
    fixture->archive.input = (AbSpan){fixture->bytes, offset + payload.size};
    fixture->archive.entries = fixture->entries;
    return true;
}

static size_t put_string(uint8_t *bytes, const char *text) {
    size_t count = strlen(text);
    put16be(bytes, (uint16_t)count);
    memcpy(bytes + 2, text, count);
    return count + 2;
}

static bool add_metadata(Fixture *fixture, const char *path, const char *texture,
                         const char *name, uint16_t x, bool repeated) {
    uint8_t bytes[256] = {0};
    memcpy(bytes, "KA3D", 4);
    memcpy(bytes + 8, "SPRT", 4);
    put16be(bytes + 16, 1);
    size_t offset = 18 + put_string(bytes + 18, texture);
    put16be(bytes + offset, repeated ? 2 : 1);
    offset += 2;
    for (size_t index = 0; index < (repeated ? 2u : 1u); ++index) {
        offset += put_string(bytes + offset, name);
        put16be(bytes + offset, (uint16_t)(x + index));
        put16be(bytes + offset + 4, 1);
        put16be(bytes + offset + 6, 1);
        offset += 12;
    }
    put32(bytes + 4, (uint32_t)offset - 8, true);
    put32(bytes + 12, (uint32_t)offset - 16, true);
    return add_entry(fixture, path, (AbSpan){bytes, offset});
}

static bool add_pvr(Fixture *fixture, const char *path, uint8_t red) {
    uint8_t bytes[56] = {0};
    put32(bytes, 0x03525650, false);
    memcpy(bytes + 8, "rgba", 4);
    put32(bytes + 12, 0x08080808, false);
    for (size_t offset = 24; offset <= 44; offset += 4) {
        put32(bytes + offset, 1, false);
    }
    bytes[52] = red;
    bytes[55] = 255;
    return add_entry(fixture, path, (AbSpan){bytes, sizeof(bytes)});
}

static size_t png_chunk(uint8_t *bytes, const char *name, AbSpan data) {
    put32(bytes, (uint32_t)data.size, true);
    memcpy(bytes + 4, name, 4);
    if (data.size) {
        memcpy(bytes + 8, data.data, data.size);
    }
    put32(bytes + 8 + data.size, ab_crc32((AbSpan){bytes + 4, data.size + 4}), true);
    return data.size + 12;
}

static bool add_png(Fixture *fixture) {
    uint8_t bytes[128] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
    uint8_t header[13] = {0};
    put32(header, 1, true);
    put32(header + 4, 1, true);
    header[8] = 8;
    header[9] = 2;
    size_t offset = 8 + png_chunk(bytes + 8, "IHDR", (AbSpan){header, 13});
    /* Authored RGB scanline (0, 10, 20, 30), one stored DEFLATE block. */
    const uint8_t compressed[] = {0x78, 1,  1,  4, 0,    0xfb, 0xff, 0,
                                  10,   20, 30, 0, 0x68, 0,    0x3d};
    offset +=
        png_chunk(bytes + offset, "IDAT", (AbSpan){compressed, sizeof(compressed)});
    offset += png_chunk(bytes + offset, "IEND", (AbSpan){0});
    return add_entry(fixture, "tiny.png", (AbSpan){bytes, offset});
}

static bool check_lookup(AbSpriteAtlas *atlas) {
    AbSpriteAtlasBinding first, other, png;
    CHECK(ab_sprite_atlas_find(atlas, string("shared"), &first) &&
          first.sprite.region->x == 2 && first.image_index == 0 &&
          same(first.sprite.metadata_path, "dir/first.dat"));
    CHECK(ab_sprite_atlas_find(atlas, string("other"), &other) &&
          other.image_index == 1 && first.image != other.image);
    CHECK(ab_sprite_atlas_find(atlas, string("png"), &png) && png.image_index == 2);
    CHECK(first.alpha == AB_SPRITE_ALPHA_PRESENT &&
          other.alpha == AB_SPRITE_ALPHA_PRESENT &&
          png.alpha == AB_SPRITE_ALPHA_UNKNOWN);
    CHECK(first.image->width == 1 && first.image->height == 1 &&
          first.image->rgba.size == 4 && first.image->rgba.data[0] == 11 &&
          other.image->rgba.data[0] == 77 &&
          memcmp(png.image->rgba.data, (uint8_t[4]){10, 20, 30, 255}, 4) == 0);
    AbSpriteAtlasImage image;
    CHECK(ab_sprite_atlas_image(atlas, 0, &image) && image.image == first.image &&
          same(image.path, "dir/atlas.pvr"));
    CHECK(ab_sprite_atlas_image(atlas, 1, &image) && image.image == other.image &&
          same(image.path, "other/atlas.pvr"));
    CHECK(ab_sprite_atlas_image(atlas, 2, &image) && image.image == png.image &&
          same(image.path, "tiny.png") && image.alpha == AB_SPRITE_ALPHA_UNKNOWN);
    CHECK(!ab_sprite_atlas_image(atlas, 3, &image) && image.image == png.image &&
          !ab_sprite_atlas_image(NULL, 0, &image) && image.image == png.image &&
          !ab_sprite_atlas_image(atlas, 0, NULL));
    CHECK(!ab_sprite_atlas_find(atlas, string("absent"), &first) &&
          first.sprite.region->x == 2 && first.image_index == 0 &&
          !ab_sprite_atlas_find(atlas, (AbSpan){NULL, 1}, &first) &&
          !ab_sprite_atlas_find(NULL, string("shared"), &first) &&
          !ab_sprite_atlas_find(atlas, string("shared"), NULL));
    return true;
}

static bool registry_checks(AbSpriteSet *set, AbSpriteAtlas *atlas) {
    AbSpriteRegistryEntry entries[4];
    AbSpriteRegistry registry = {entries, 0, 3};
    AbSpriteAtlasBinding binding;
    CHECK(
        ab_sprite_set_register_sheet(set, 0, string("first"), &registry, NULL) &&
        registry.count == 1 &&
        ab_sprite_set_register_sheet(set, 1, string("second"), &registry, NULL) &&
        ab_sprite_atlas_find_registered(atlas, &registry, string("shared"), &binding) &&
        binding.sprite.region->x == 3 && binding.image_index == 0 &&
        same(binding.sprite.metadata_path, "dir/second.dat"));
    CHECK(
        ab_sprite_set_register_sheet(set, 0, string("repeat"), &registry, NULL) &&
        registry.count == 3 &&
        ab_sprite_atlas_find_registered(atlas, &registry, string("shared"), &binding) &&
        binding.sprite.region->x == 2);
    CHECK(!ab_sprite_set_register_sheet(set, 2, string("full"), &registry, NULL) &&
          registry.count == 3);
    registry.capacity = 4;
    uint32_t token = 0;
    AbSpriteRegistryEntry composite = {string("shared"), string("composite"),
                                       AB_SPRITE_REGISTRY_COMPOSITE, &token};
    CHECK(ab_sprite_registry_add(&registry, composite, NULL) &&
          !ab_sprite_atlas_find_registered(atlas, &registry, string("shared"),
                                           &binding) &&
          binding.sprite.region->x == 2);
    CHECK(
        ab_sprite_set_unregister_sheet(set, 0, &registry, NULL) &&
        registry.count == 2 &&
        !ab_sprite_atlas_find_registered(atlas, &registry, string("shared"), &binding));
    CHECK(
        ab_sprite_registry_remove(&registry, composite.name, composite.kind,
                                  composite.resource, NULL) &&
        ab_sprite_atlas_find_registered(atlas, &registry, string("shared"), &binding) &&
        binding.sprite.region->x == 3);
    composite.kind = AB_SPRITE_REGISTRY_SPRITE;
    CHECK(ab_sprite_registry_add(&registry, composite, NULL) &&
          !ab_sprite_atlas_find_registered(atlas, &registry, string("shared"),
                                           &binding) &&
          binding.sprite.region->x == 3);
    CHECK(ab_sprite_registry_remove(&registry, composite.name, composite.kind,
                                    composite.resource, NULL) &&
          ab_sprite_set_unregister_sheet(set, 1, &registry, NULL) && !registry.count);
    CHECK(
        !ab_sprite_set_register_sheet(set, 5, string("bad"), &registry, NULL) &&
        !ab_sprite_set_register_sheet(set, 0, (AbSpan){NULL, 1}, &registry, NULL) &&
        !ab_sprite_set_unregister_sheet(set, 0, NULL, NULL) &&
        !ab_sprite_atlas_find_registered(NULL, &registry, string("shared"), &binding) &&
        !ab_sprite_atlas_find_registered(atlas, NULL, string("shared"), &binding));
    return true;
}

static bool owner_checks(void) {
    Fixture fixture = {0};
    CHECK(add_metadata(&fixture, "dir/first.dat", "atlas.pvr", "shared", 1, true) &&
          add_metadata(&fixture, "dir/second.dat", "atlas.pvr", "shared", 3, false) &&
          add_metadata(&fixture, "other/third.dat", "atlas.pvr", "other", 0, false) &&
          add_metadata(&fixture, "png.dat", "tiny.png", "png", 0, false) &&
          add_pvr(&fixture, "dir/atlas.pvr", 11) &&
          add_pvr(&fixture, "other/atlas.pvr", 77) && add_png(&fixture));
    const char *paths[] = {"dir/first.dat", "dir/second.dat", "other/third.dat",
                           "png.dat", "dir/first.dat"};
    AbSpriteSet *set = NULL;
    AbSpriteAtlas *atlas = NULL;
    AbError error = {0};
    CHECK(ab_sprite_set_open(&fixture.archive, paths, 5, 16384, &set, &error));
    CHECK(ab_sprite_set_count(set) == 5 && ab_sprite_set_count(NULL) == 0);
    AbSpriteSheetView view = {0};
    CHECK(ab_sprite_set_sheet(set, 1, &view) && same(view.metadata_path, paths[1]));
    const AbSpriteSheet *sheet = view.sheet;
    CHECK(!ab_sprite_set_sheet(set, 5, &view) && view.sheet == sheet &&
          !ab_sprite_set_sheet(NULL, 0, &view) && view.sheet == sheet &&
          !ab_sprite_set_sheet(set, 0, NULL));
    CHECK(!ab_sprite_atlas_open(NULL, set, 16384, 1024, &atlas, &error) && !atlas &&
          !ab_sprite_atlas_open(&fixture.archive, NULL, 16384, 1024, &atlas, &error) &&
          !atlas &&
          !ab_sprite_atlas_open(&fixture.archive, set, 16384, 1024, NULL, &error));
    CHECK(!ab_sprite_atlas_open(&fixture.archive, set, 1, 1024, &atlas, &error) &&
          !atlas);
    CHECK(ab_sprite_atlas_open(&fixture.archive, set, 16384, 1024, &atlas, &error) &&
          ab_sprite_atlas_count(atlas) == 3 && check_lookup(atlas) &&
          registry_checks(set, atlas));
    size_t bytes = ab_sprite_atlas_bytes(atlas);
    ab_sprite_atlas_close(atlas);
    atlas = NULL;
    CHECK(
        bytes > 12 &&
        !ab_sprite_atlas_open(&fixture.archive, set, bytes - 1, 1024, &atlas, &error) &&
        !atlas);
    CHECK(ab_sprite_atlas_open(&fixture.archive, set, bytes, 1024, &atlas, &error) &&
          ab_sprite_atlas_bytes(atlas) == bytes && check_lookup(atlas));
    ab_sprite_atlas_close(atlas);
    atlas = NULL;
    CHECK(!ab_sprite_atlas_open(&fixture.archive, set, 16384, 55, &atlas, &error) &&
          !atlas);
    /* A later decode fails after the first atlas has already been retained. */
    fixture.bytes[fixture.entries[5].data_offset + 52] ^= 1;
    CHECK(!ab_sprite_atlas_open(&fixture.archive, set, 16384, 1024, &atlas, &error) &&
          !atlas);
    fixture.bytes[fixture.entries[5].data_offset + 52] ^= 1;
    --fixture.archive.count;
    CHECK(!ab_sprite_atlas_open(&fixture.archive, set, 16384, 1024, &atlas, &error) &&
          !atlas);
    ++fixture.archive.count;
    ab_sprite_set_close(set);
    CHECK(ab_sprite_set_open(&fixture.archive, NULL, 0, 16384, &set, &error) &&
          ab_sprite_atlas_open(&fixture.archive, set, 16384, 0, &atlas, &error) &&
          ab_sprite_atlas_count(atlas) == 0);
    ab_sprite_atlas_close(atlas);
    ab_sprite_set_close(set);
    CHECK(ab_sprite_atlas_bytes(NULL) == 0 && ab_sprite_atlas_count(NULL) == 0);
    ab_sprite_atlas_close(NULL);
    const char *unsupported[] = {
        "../atlas.pvr", "dir/atlas.pvr", "dir\\atlas.pvr", ".", "..", "",
        "unknown.webp"};
    for (size_t index = 0; index < sizeof(unsupported) / sizeof(*unsupported);
         ++index) {
        Fixture invalid = {0};
        CHECK(add_metadata(&invalid, "test.dat", unsupported[index], "test", 0, false));
        if (index + 1 == sizeof(unsupported) / sizeof(*unsupported)) {
            CHECK(add_entry(&invalid, "unknown.webp", string("unsupported")));
        }
        CHECK(ab_sprite_set_open(&invalid.archive, (const char *[]){"test.dat"}, 1,
                                 16384, &set, &error));
        CHECK(
            !ab_sprite_atlas_open(&invalid.archive, set, 16384, 1024, &atlas, &error) &&
            !atlas);
        ab_sprite_set_close(set);
    }
    puts("Atlas ownership, shared images, lookup order, decoding and bounds pass.");
    return true;
}

static uint32_t float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool native_rows(FILE *file, const AbSpriteAtlas *atlas) {
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_SPRITE_BINDINGS_V1 22\n") == 0);
    for (size_t item = 0; item < 22; ++item) {
        char name[128], path[256], object[128];
        CHECK(fscanf(file, "%127s %127s %255s\n", object, name, path) == 3);
        int32_t region[4];
        uint32_t bits[10];
        for (size_t index = 0; index < 4; ++index) {
            CHECK(fscanf(file, "%" SCNd32, region + index) == 1 && fgetc(file) == ' ');
        }
        for (size_t index = 0; index < 10; ++index) {
            CHECK(fscanf(file, "%" SCNx32, bits + index) == 1 &&
                  fgetc(file) == (index == 9 ? '\n' : ' '));
        }
        AbSpriteAtlasBinding binding;
        CHECK(ab_sprite_atlas_find(atlas, string(name), &binding));
        AbSpriteAtlasImage image;
        CHECK(ab_sprite_atlas_image(atlas, binding.image_index, &image) &&
              image.image == binding.image && same(image.path, path));
        const AbSpriteRegion *source = binding.sprite.region;
        CHECK(source->x == region[0] && source->y == region[1] &&
              source->width == region[2] && source->height == region[3] &&
              float_bits((float)source->pivot_x) == bits[0] &&
              float_bits((float)source->pivot_y) == bits[1]);
        AbDrawQuad quad;
        CHECK(ab_sprite_quad(source, binding.image->width, binding.image->height,
                             (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad,
                             NULL));
        for (size_t index = 0; index < 4; ++index) {
            if (float_bits(quad.vertices[index].u) != bits[2 + index * 2] ||
                float_bits(quad.vertices[index].v) != bits[3 + index * 2]) {
                fprintf(stderr, "Native atlas UV mismatch for %s vertex %zu\n", object,
                        index);
                return false;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB sprite bindings complete\n") == 0 && fgetc(file) == EOF &&
          !ferror(file));
    puts("All 22 native object sprite paths, regions, pivots and UVs match the APK.");
    return true;
}

static bool native_checks(const AbZip *archive, FILE *file) {
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat"};
    AbSpriteSet *set = NULL;
    AbSpriteAtlas *atlas = NULL;
    AbError error = {0};
    bool okay = ab_sprite_set_open(archive, paths, 3, 1024 * 1024, &set, &error) &&
                ab_sprite_atlas_open(archive, set, 16 * 1024 * 1024, 8 * 1024 * 1024,
                                     &atlas, &error) &&
                ab_sprite_atlas_count(atlas) == 3 && native_rows(file, atlas);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_sprite_atlas_close(atlas);
    ab_sprite_set_close(set);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return owner_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_sprite_atlas_tests [APK NATIVE_BINDINGS]\n");
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    FILE *file = fopen(argv[2], "rb");
    bool okay = file && ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                native_checks(&archive, file);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
