#include "ab/aes.h"
#include "ab/axml.h"
#include "ab/composite.h"
#include "ab/font.h"
#include "ab/hash.h"
#include "ab/image.h"
#include "ab/json.h"
#include "ab/lua_chunk.h"
#include "ab/lua_data.h"
#include "ab/pack.h"
#include "ab/sprite.h"
#include "ab/text.h"
#include "ab/target_resources.h"
#include "ab/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { APK_LIMIT = 128 * 1024 * 1024, ENTRY_LIMIT = 32 * 1024 * 1024 };

typedef enum { AUDIT_ARCHIVE, AUDIT_PNG, AUDIT_PVR } AuditKind;
typedef enum { DAT_SPRITES, DAT_FONTS, DAT_TEXT, DAT_COMPOSITES } DatKind;

static const char target_digest[] =
    "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad";

static void print_json_string(AbSpan value) {
    ab_json_bytes(stdout, value);
}

static void print_text(const char *value) {
    print_json_string((AbSpan){(const uint8_t *)value, strlen(value)});
}

static bool digest_hex(AbSpan input, char output[65], AbError *error) {
    uint8_t digest[32];
    if (!ab_sha256(input, digest, error)) {
        return false;
    }
    ab_digest_hex(digest, output);
    return true;
}

static bool read_manifest(const AbZip *zip, AbManifest *manifest, AbError *error) {
    const AbZipEntry *entry = ab_zip_find(zip, "AndroidManifest.xml");
    if (!entry) {
        return ab_fail(error, "APK has no AndroidManifest.xml");
    }
    AbBuffer bytes = {0};
    size_t index = (size_t)(entry - zip->entries);
    if (!ab_zip_read(zip, index, ENTRY_LIMIT, &bytes, error)) {
        return false;
    }
    bool okay = ab_manifest_read((AbSpan){bytes.data, bytes.size}, manifest, error);
    ab_buffer_free(&bytes);
    return okay;
}

static bool print_manifest(const AbZip *zip, AbError *error) {
    AbManifest manifest;
    if (!read_manifest(zip, &manifest, error)) {
        return false;
    }
    puts("{");
    printf("  \"package\": ");
    print_text(manifest.package);
    printf(",\n  \"version_name\": ");
    print_text(manifest.version_name);
    printf(",\n  \"version_code\": %u,\n", manifest.version_code);
    printf("  \"min_sdk\": %u,\n", manifest.min_sdk);
    printf("  \"target_sdk\": %u,\n", manifest.target_sdk);
    printf("  \"gles_version\": %u,\n", manifest.gles_version);
    printf("  \"launcher\": ");
    print_text(manifest.launcher);
    printf(",\n  \"activity_count\": %zu,\n", manifest.activity_count);
    printf("  \"service_count\": %zu,\n", manifest.service_count);
    printf("  \"receiver_count\": %zu\n", manifest.receiver_count);
    puts("}");
    return true;
}

static bool inventory(const AbZip *zip, const char *apk_digest, AuditKind kind,
                      AbError *error) {
    bool image_only = kind != AUDIT_ARCHIVE;
    const char *suffix = kind == AUDIT_PNG ? ".png" : ".pvr";
    if (image_only && strcmp(apk_digest, target_digest) != 0) {
        return ab_fail(error, "Image audit requires the pinned APK hash");
    }
    printf("{\n  \"apk_sha256\": \"%s\",\n  \"entries\": [\n", apk_digest);
    size_t emitted = 0;
    for (size_t index = 0; index < zip->count; ++index) {
        const AbZipEntry *entry = zip->entries + index;
        if (image_only &&
            (entry->name.size < 4 ||
             memcmp(entry->name.data + entry->name.size - 4, suffix, 4) != 0)) {
            continue;
        }
        AbBuffer bytes = {0};
        char digest[65];
        if (!ab_zip_read(zip, index, ENTRY_LIMIT, &bytes, error)) {
            return false;
        }
        bool okay = digest_hex((AbSpan){bytes.data, bytes.size}, digest, error);
        AbImage image = {0};
        char pixel_digest[65] = {0};
        if (okay && image_only) {
            AbSpan encoded = {bytes.data, bytes.size};
            okay = kind == AUDIT_PNG
                       ? ab_png_decode(encoded, ENTRY_LIMIT, &image, error)
                       : ab_pvr_decode(encoded, ENTRY_LIMIT, &image, error);
            if (okay) {
                okay = digest_hex((AbSpan){image.rgba.data, image.rgba.size},
                                  pixel_digest, error);
            }
        }
        uint32_t width = image.width;
        uint32_t height = image.height;
        ab_image_free(&image);
        ab_buffer_free(&bytes);
        if (!okay) {
            fprintf(stderr, "Image or archive entry index: %zu\n", index);
            return false;
        }
        if (emitted) {
            puts(",");
        }
        printf("    {\n      \"path\": ");
        print_json_string(entry->name);
        printf(",\n      \"size\": %u,\n", entry->size);
        printf("      \"compressed_size\": %u,\n", entry->compressed_size);
        printf("      \"method\": %u,\n", entry->method);
        printf("      \"crc32\": \"%08x\",\n", entry->crc32);
        if (image_only) {
            printf("      \"width\": %u,\n      \"height\": %u,\n", width, height);
            printf("      \"rgba_sha256\": \"%s\",\n", pixel_digest);
        }
        printf("      \"sha256\": \"%s\"\n    }", digest);
        ++emitted;
    }
    printf("\n  ],\n  \"entry_count\": %zu\n}\n", emitted);
    size_t expected = kind == AUDIT_PNG ? 786 : 243;
    return !image_only || emitted == expected ||
           ab_fail(error, "Image count differs from target inventory");
}

static void print_sprite_sheet(const AbZipEntry *entry, const AbSpriteSheet *sheet) {
    printf("    {\n      \"path\": ");
    print_json_string(entry->name);
    printf(",\n      \"version\": 1,\n      \"texture\": ");
    print_json_string(sheet->texture);
    printf(",\n      \"sprites\": [");
    for (size_t index = 0; index < sheet->count; ++index) {
        const AbSpriteRegion *region = sheet->sprites + index;
        printf("%s\n        {\"name\": ", index ? "," : "");
        print_json_string(region->name);
        printf(", \"fields\": [%d, %d, %d, %d, %d, %d]}", region->x, region->y,
               region->width, region->height, region->pivot_x, region->pivot_y);
    }
    printf("\n      ]\n    }");
}

static void print_font(const AbZipEntry *entry, const AbBitmapFont *font) {
    printf("    {\n      \"path\": ");
    print_json_string(entry->name);
    printf(",\n      \"version\": %u,\n      \"texture\": ", font->version);
    print_json_string(font->texture);
    printf(",\n      \"metrics\": [%d, %d],\n      \"glyphs\": [", font->leading,
           font->tracking);
    for (size_t index = 0; index < font->count; ++index) {
        const AbFontGlyph *glyph = font->glyphs + index;
        printf("%s\n        {\"code\": %u, \"fields\": [%d, %d, %d, %d, %d]}",
               index ? "," : "", glyph->code, glyph->x, glyph->y, glyph->width,
               glyph->height, glyph->pivot_y);
    }
    printf("\n      ]\n    }");
}

static void print_span_list(const AbSpan *strings, size_t count) {
    printf("[");
    for (size_t index = 0; index < count; ++index) {
        if (index) {
            printf(", ");
        }
        print_json_string(strings[index]);
    }
    printf("]");
}

static void print_text_catalog(const AbZipEntry *entry, const AbTextCatalog *catalog) {
    printf("    {\n      \"path\": ");
    print_json_string(entry->name);
    printf(",\n      \"locales\": ");
    print_span_list(catalog->locales, catalog->locale_count);
    printf(",\n      \"ids\": ");
    print_span_list(catalog->ids, catalog->id_count);
    printf(",\n      \"translations\": [");
    for (size_t index = 0; index < catalog->locale_count; ++index) {
        printf("%s\n        ", index ? "," : "");
        print_span_list(catalog->translations + index * catalog->id_count,
                        catalog->id_count);
    }
    printf("\n      ]\n    }");
}

static void print_composites(const AbZipEntry *entry, const AbCompositeSet *set) {
    printf("    {\n      \"path\": ");
    print_json_string(entry->name);
    printf(",\n      \"root\": \"%s\",\n      \"version\": %u,\n",
           set->extended ? "RVIO" : "KA3D", set->version);
    printf("      \"composites\": [");
    for (size_t index = 0; index < set->count; ++index) {
        const AbComposite *composite = set->composites + index;
        printf("%s\n        {\"name\": ", index ? "," : "");
        print_json_string(composite->name);
        printf(", \"parts\": [");
        for (size_t part_index = 0; part_index < composite->part_count; ++part_index) {
            const AbCompositePart *part = composite->parts + part_index;
            printf("%s\n          {\"sprite\": ", part_index ? "," : "");
            print_json_string(part->sprite);
            printf(", \"tag\": ");
            print_json_string(part->tag);
            printf(", \"position\": [%d, %d], \"transform_bits\": "
                   "[\"%08x\", \"%08x\", \"%08x\"], \"flip\": [%s, %s]}",
                   part->x, part->y, part->scale_x_bits, part->scale_y_bits,
                   part->rotation_bits, part->flip_x ? "true" : "false",
                   part->flip_y ? "true" : "false");
        }
        printf("\n        ], \"points\": [");
        for (size_t point_index = 0; point_index < composite->point_count;
             ++point_index) {
            const AbCompositePoint *point = composite->points + point_index;
            printf("%s{\"name\": ", point_index ? ", " : "");
            print_json_string(point->name);
            printf(", \"position\": [%d, %d]}", point->x, point->y);
        }
        printf("]}");
    }
    printf("\n      ]\n    }");
}

static bool inventory_dat(const AbZip *zip, const char *digest, DatKind kind,
                          AbError *error) {
    if (strcmp(digest, target_digest) != 0) {
        return ab_fail(error, "DAT audit requires the pinned APK hash");
    }
    printf("{\n  \"apk_sha256\": \"%s\",\n  \"entries\": [\n", digest);
    size_t sheets = 0;
    size_t items = 0;
    for (size_t index = 0; index < zip->count; ++index) {
        const AbZipEntry *entry = zip->entries + index;
        if (entry->name.size < 4 ||
            memcmp(entry->name.data + entry->name.size - 4, ".dat", 4) != 0) {
            continue;
        }
        AbBuffer bytes = {0};
        if (!ab_zip_read(zip, index, ENTRY_LIMIT, &bytes, error)) {
            return false;
        }
        const char *tags[] = {"SPRT", "FONT", "TEXT", "COMP"};
        /* The pinned archive's matching DAT files start with this chunk. */
        if (bytes.size < 16 ||
            (memcmp(bytes.data, "KA3D", 4) != 0 &&
             (kind != DAT_COMPOSITES || memcmp(bytes.data, "RVIO", 4) != 0)) ||
            memcmp(bytes.data + 8, tags[kind], 4) != 0) {
            ab_buffer_free(&bytes);
            continue;
        }
        AbSpriteSheet sheet = {0};
        AbBitmapFont font = {0};
        AbTextCatalog catalog = {0};
        AbCompositeSet composites = {0};
        AbSpan input = {bytes.data, bytes.size};
        bool okay;
        if (kind == DAT_COMPOSITES) {
            okay = ab_composite_read(input, ENTRY_LIMIT, &composites, error);
        } else if (kind == DAT_TEXT) {
            okay = ab_text_read(input, ENTRY_LIMIT, &catalog, error);
        } else if (kind == DAT_FONTS) {
            okay = ab_font_read(input, ENTRY_LIMIT, &font, error);
        } else {
            okay = ab_sprite_sheet_read(input, ENTRY_LIMIT, &sheet, error);
        }
        if (okay) {
            if (sheets) {
                puts(",");
            }
            if (kind == DAT_COMPOSITES) {
                print_composites(entry, &composites);
                items += composites.count;
            } else if (kind == DAT_TEXT) {
                print_text_catalog(entry, &catalog);
                items += catalog.locale_count * catalog.id_count;
            } else if (kind == DAT_FONTS) {
                print_font(entry, &font);
                items += font.count;
            } else {
                print_sprite_sheet(entry, &sheet);
                items += sheet.count;
            }
            ++sheets;
        }
        ab_composite_free(&composites);
        ab_text_free(&catalog);
        ab_font_free(&font);
        ab_sprite_sheet_free(&sheet);
        ab_buffer_free(&bytes);
        if (!okay) {
            fprintf(stderr, "DAT entry index: %zu\n", index);
            return false;
        }
    }
    const char *item_names[] = {"sprite", "glyph", "translation", "composite"};
    const size_t expected_sheets[] = {237, 37, 4, 52};
    const size_t expected_items[] = {5089, 12577, 4576, 842};
    printf("\n  ],\n  \"sheet_count\": %zu,\n  \"%s_count\": %zu\n}\n", sheets,
           item_names[kind], items);
    return (sheets == expected_sheets[kind] && items == expected_items[kind]) ||
           ab_fail(error, "DAT counts differ from target inventory");
}

static bool verify(const AbZip *zip, const char *digest, AbError *error) {
    if (strcmp(digest, target_digest) != 0) {
        return ab_fail(error,
                       "APK SHA-256 does not match the pinned Android 5.2.0 build");
    }
    AbManifest manifest;
    if (!read_manifest(zip, &manifest, error)) {
        return false;
    }
    if (strcmp(manifest.package, "com.rovio.angrybirds") != 0 ||
        strcmp(manifest.version_name, "5.2.0") != 0 ||
        strcmp(manifest.launcher, "com.rovio.fusion.App") != 0 ||
        manifest.version_code != 12200520 || manifest.min_sdk != 9 ||
        manifest.target_sdk != 22 || manifest.gles_version != 0x20000 ||
        !ab_zip_find(zip, "lib/armeabi/libAngryBirdsClassic.so") ||
        !ab_zip_find(zip, "lib/armeabi-v7a/libAngryBirdsClassic.so")) {
        return ab_fail(error,
                       "APK manifest or ABI inventory differs from target evidence");
    }
    uint64_t decoded_size = 0;
    for (size_t index = 0; index < zip->count; ++index) {
        AbBuffer bytes = {0};
        if (!ab_zip_read(zip, index, ENTRY_LIMIT, &bytes, error)) {
            fprintf(stderr, "Entry index: %zu\n", index);
            return false;
        }
        decoded_size += bytes.size;
        ab_buffer_free(&bytes);
    }
    printf("Verified APK SHA-256: %s\n", digest);
    printf("Verified manifest: %s %s (%u), API %u/%u\n", manifest.package,
           manifest.version_name, manifest.version_code, manifest.min_sdk,
           manifest.target_sdk);
    printf("Verified ZIP structure, decompression, and CRC-32 for %zu entries; "
           "%llu decoded bytes.\n",
           zip->count, (unsigned long long)decoded_size);
    return true;
}

static bool extract(const AbZip *zip, const char *name, const char *path,
                    AbError *error) {
    const AbZipEntry *entry = ab_zip_find(zip, name);
    if (!entry) {
        return ab_fail(error, "Requested entry is absent from APK");
    }
    AbBuffer bytes = {0};
    if (!ab_zip_read(zip, (size_t)(entry - zip->entries), ENTRY_LIMIT, &bytes, error)) {
        return false;
    }
    bool okay = ab_file_write(path, (AbSpan){bytes.data, bytes.size}, error);
    ab_buffer_free(&bytes);
    return okay;
}

static bool recover_lua_entry(const AbZip *zip, size_t index, AbSpan key,
                              AbBuffer *output, AbError *error) {
    AbBuffer encrypted = {0};
    AbBuffer decrypted = {0};
    *output = (AbBuffer){0};
    bool okay = ab_zip_read(zip, index, ENTRY_LIMIT, &encrypted, error) &&
                ab_asset_decrypt((AbSpan){encrypted.data, encrypted.size}, key,
                                 &decrypted, error) &&
                ab_asset_unpack((AbSpan){decrypted.data, decrypted.size}, ENTRY_LIMIT,
                                output, error);
    ab_buffer_free(&decrypted);
    ab_buffer_free(&encrypted);
    return okay;
}

static void print_lua_matches(const AbZipEntry *entry, const AbLuaPrototype *prototype,
                              AbSpan needle, size_t *matches) {
    for (size_t index = 0; index < prototype->constant_count; ++index) {
        const AbLuaConstant *constant = prototype->constants + index;
        if (constant->kind != AB_LUA_STRING || constant->string.size != needle.size ||
            (needle.size && memcmp(constant->string.data, needle.data, needle.size))) {
            continue;
        }
        printf("%s    {\n      \"path\": ", *matches ? ",\n" : "");
        print_json_string(entry->name);
        printf(",\n      \"prototype_offset\": %zu,\n      \"constant_index\": %zu,\n",
               prototype->file_offset, index);
        printf("      \"global_references\": [");
        size_t references = 0;
        for (size_t pc = 0; pc < prototype->instructions.size / 4; ++pc) {
            uint32_t word = ab_u32le(prototype->instructions.data + pc * 4);
            unsigned opcode = word & 63;
            if ((opcode == 5 || opcode == 7) && word >> 14 == index) {
                printf("%s{\"instruction\": %zu, \"operation\": \"%s\"}",
                       references ? ", " : "", pc, opcode == 5 ? "read" : "write");
                ++references;
            }
            if (opcode == 34 && !((word >> 14) & 511)) {
                ++pc;
            }
        }
        printf("]\n    }");
        ++*matches;
    }
    for (size_t index = 0; index < prototype->child_count; ++index) {
        print_lua_matches(entry, prototype->children + index, needle, matches);
    }
}

static bool load_resource_key(const AbZip *zip, const char *path, AbBuffer *output,
                              AbError *error) {
    if (path) {
        if (!ab_file_read(path, 32, output, error)) {
            return false;
        }
        if (output->size == 32) {
            return true;
        }
        ab_buffer_free(output);
        return ab_fail(error, "Target resource key must contain exactly 32 raw bytes");
    }
    output->data = malloc(32);
    if (!output->data) {
        return ab_fail(error, "Cannot allocate target resource-key storage");
    }
    output->size = 32;
    if (!ab_target_resource_key(zip, ENTRY_LIMIT, output->data, error)) {
        ab_buffer_free(output);
        return false;
    }
    return true;
}

static bool inventory_lua(const AbZip *zip, const char *apk_digest,
                          const char *key_path, bool levels_only, const char *needle,
                          AbError *error) {
    if (strcmp(apk_digest, target_digest) != 0) {
        return ab_fail(error, "Lua recovery requires the pinned APK hash");
    }
    AbBuffer key = {0};
    if (!load_resource_key(zip, key_path, &key, error)) {
        return false;
    }
    bool okay = false;
    printf("{\n  \"apk_sha256\": \"%s\",\n  \"entries\": [\n", apk_digest);
    size_t recovered = 0;
    size_t matches = 0;
    uint64_t decoded_bytes = 0;
    for (size_t index = 0; index < zip->count; ++index) {
        const AbZipEntry *entry = zip->entries + index;
        static const char level_prefix[] = "assets/data/levels/";
        if (levels_only &&
            (entry->name.size < sizeof(level_prefix) - 1 ||
             memcmp(entry->name.data, level_prefix, sizeof(level_prefix) - 1) != 0)) {
            continue;
        }
        if (entry->name.size < 4 ||
            memcmp(entry->name.data + entry->name.size - 4, ".lua", 4) != 0) {
            continue;
        }
        AbBuffer bytes = {0};
        char digest[65];
        if (!recover_lua_entry(zip, index, (AbSpan){key.data, key.size}, &bytes,
                               error)) {
            fprintf(stderr, "Lua entry index: %zu\n", index);
            goto release_key;
        }
        AbLuaChunk chunk = {0};
        bool supported =
            ab_lua_read((AbSpan){bytes.data, bytes.size}, ENTRY_LIMIT, &chunk, error);
        size_t functions = chunk.function_count;
        size_t instructions = chunk.instruction_count;
        size_t constants = chunk.constant_count;
        AbLuaData data = {0};
        if (supported && levels_only) {
            supported = ab_lua_data(&chunk, ENTRY_LIMIT, &data, error);
        }
        size_t tables = data.table_count;
        size_t globals = data.globals ? data.globals->count : 0;
        if (supported && needle) {
            print_lua_matches(entry, &chunk.root,
                              (AbSpan){(const uint8_t *)needle, strlen(needle)},
                              &matches);
        }
        ab_lua_data_close(&data);
        ab_lua_close(&chunk);
        bool hashed = digest_hex((AbSpan){bytes.data, bytes.size}, digest, error);
        size_t size = bytes.size;
        ab_buffer_free(&bytes);
        if (!hashed || !supported) {
            fprintf(stderr, "Lua structure entry index: %zu\n", index);
            goto release_key;
        }
        if (!needle && recovered) {
            puts(",");
        }
        if (!needle) {
            printf("    {\n      \"path\": ");
            print_json_string(entry->name);
            printf(",\n      \"decoded_size\": %zu,\n", size);
            printf("      \"function_count\": %zu,\n", functions);
            printf("      \"instruction_count\": %zu,\n", instructions);
            printf("      \"constant_count\": %zu,\n", constants);
            if (levels_only) {
                printf("      \"data_table_count\": %zu,\n", tables);
                printf("      \"global_count\": %zu,\n", globals);
            }
            printf("      \"decoded_sha256\": \"%s\"\n    }", digest);
        }
        ++recovered;
        decoded_bytes += size;
    }
    printf("\n  ],\n  \"recovered_count\": %zu,\n", recovered);
    if (needle) {
        printf("  \"query\": ");
        print_text(needle);
        printf(",\n  \"match_count\": %zu,\n", matches);
    }
    printf("  \"decoded_bytes\": %llu\n}\n", (unsigned long long)decoded_bytes);
    okay = recovered == (levels_only ? 655 : 1529) ||
           ab_fail(error, "Lua entry count differs from target inventory");

release_key:
    memset(key.data, 0, key.size);
    ab_buffer_free(&key);
    return okay;
}

static bool decrypt_asset(const AbZip *zip, const char *apk_digest, const char *name,
                          const char *key_path, const char *path, bool decompress,
                          AbError *error) {
    if (strcmp(apk_digest, target_digest) != 0) {
        return ab_fail(error, "Asset recovery requires the pinned APK hash");
    }
    const AbZipEntry *entry = ab_zip_find(zip, name);
    if (!entry) {
        return ab_fail(error, "Requested entry is absent from APK");
    }
    AbBuffer key = {0};
    AbBuffer encrypted = {0};
    AbBuffer decrypted = {0};
    AbBuffer decoded = {0};
    bool okay = false;
    if (!load_resource_key(zip, key_path, &key, error)) {
        goto release_buffers;
    }
    if (decompress) {
        if (!ab_target_packed_read(zip, name, (AbSpan){key.data, key.size}, ENTRY_LIMIT,
                                   &decoded, error)) {
            goto release_buffers;
        }
        okay = ab_file_write(path, (AbSpan){decoded.data, decoded.size}, error);
    } else {
        if (!ab_zip_read(zip, (size_t)(entry - zip->entries), ENTRY_LIMIT, &encrypted,
                         error) ||
            !ab_asset_decrypt((AbSpan){encrypted.data, encrypted.size},
                              (AbSpan){key.data, key.size}, &decrypted, error)) {
            goto release_buffers;
        }
        okay = ab_file_write(path, (AbSpan){decrypted.data, decrypted.size}, error);
    }

release_buffers:
    ab_buffer_free(&decoded);
    ab_buffer_free(&decrypted);
    ab_buffer_free(&encrypted);
    if (key.data) {
        memset(key.data, 0, key.size);
    }
    ab_buffer_free(&key);
    return okay;
}

int main(int argc, char **argv) {
    bool standard =
        argc == 3 &&
        (strcmp(argv[1], "verify") == 0 || strcmp(argv[1], "inventory") == 0 ||
         strcmp(argv[1], "inventory-png") == 0 ||
         strcmp(argv[1], "inventory-pvr") == 0 ||
         strcmp(argv[1], "inventory-sprites") == 0 ||
         strcmp(argv[1], "inventory-fonts") == 0 ||
         strcmp(argv[1], "inventory-composites") == 0 ||
         strcmp(argv[1], "inventory-text") == 0 || strcmp(argv[1], "manifest") == 0);
    bool extraction = argc == 5 && strcmp(argv[1], "extract") == 0;
    bool decryption = (argc == 5 || argc == 6) && (strcmp(argv[1], "decrypt") == 0 ||
                                                   strcmp(argv[1], "recover-lua") == 0);
    bool lua_inventory =
        (argc == 3 || argc == 4) && (strcmp(argv[1], "inventory-lua") == 0 ||
                                     strcmp(argv[1], "inventory-levels") == 0);
    bool lua_search =
        (argc == 4 || argc == 5) && strcmp(argv[1], "find-lua-string") == 0;
    if (!standard && !extraction && !decryption && !lua_inventory && !lua_search) {
        fprintf(
            stderr,
            "Usage: ab_apk verify|inventory|inventory-png|inventory-pvr|manifest APK\n"
            "       ab_apk inventory-sprites|inventory-fonts|inventory-text APK\n"
            "       ab_apk inventory-composites APK\n"
            "       ab_apk extract APK ENTRY NEW_OUTPUT_FILE\n"
            "       ab_apk decrypt|recover-lua APK ENTRY [PRIVATE_KEY_FILE] "
            "NEW_OUTPUT_FILE\n"
            "       ab_apk inventory-lua|inventory-levels APK [PRIVATE_KEY_FILE]\n"
            "       ab_apk find-lua-string APK [PRIVATE_KEY_FILE] EXACT_STRING\n");
        return 2;
    }
    AbError error = {0};
    AbBuffer bytes = {0};
    AbZip zip = {0};
    char digest[65];
    bool okay = false;
    if (!ab_file_read(argv[2], APK_LIMIT, &bytes, &error) ||
        !digest_hex((AbSpan){bytes.data, bytes.size}, digest, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &zip, &error)) {
        goto release_input;
    }
    if (strcmp(argv[1], "verify") == 0) {
        okay = verify(&zip, digest, &error);
    } else if (strcmp(argv[1], "inventory") == 0) {
        okay = inventory(&zip, digest, AUDIT_ARCHIVE, &error);
    } else if (strcmp(argv[1], "inventory-png") == 0) {
        okay = inventory(&zip, digest, AUDIT_PNG, &error);
    } else if (strcmp(argv[1], "inventory-pvr") == 0) {
        okay = inventory(&zip, digest, AUDIT_PVR, &error);
    } else if (strcmp(argv[1], "inventory-sprites") == 0) {
        okay = inventory_dat(&zip, digest, DAT_SPRITES, &error);
    } else if (strcmp(argv[1], "inventory-fonts") == 0) {
        okay = inventory_dat(&zip, digest, DAT_FONTS, &error);
    } else if (strcmp(argv[1], "inventory-text") == 0) {
        okay = inventory_dat(&zip, digest, DAT_TEXT, &error);
    } else if (strcmp(argv[1], "inventory-composites") == 0) {
        okay = inventory_dat(&zip, digest, DAT_COMPOSITES, &error);
    } else if (strcmp(argv[1], "manifest") == 0) {
        okay = print_manifest(&zip, &error);
    } else if (lua_inventory || lua_search) {
        const char *key =
            (lua_inventory && argc == 4) || (lua_search && argc == 5) ? argv[3] : NULL;
        okay =
            inventory_lua(&zip, digest, key, strcmp(argv[1], "inventory-levels") == 0,
                          lua_search ? argv[argc - 1] : NULL, &error);
    } else if (extraction) {
        okay = extract(&zip, argv[3], argv[4], &error);
    } else {
        okay =
            decrypt_asset(&zip, digest, argv[3], argc == 6 ? argv[4] : NULL,
                          argv[argc - 1], strcmp(argv[1], "recover-lua") == 0, &error);
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        okay = ab_fail(&error, "Cannot write command output");
    }

release_input:
    ab_zip_close(&zip);
    ab_buffer_free(&bytes);
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
    }
    return okay ? 0 : 1;
}
