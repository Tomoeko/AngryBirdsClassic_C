#include "ab/aes.h"
#include "ab/axml.h"
#include "ab/composite.h"
#include "ab/dat.h"
#include "ab/font.h"
#include "ab/hash.h"
#include "ab/image.h"
#include "ab/inflate.h"
#include "ab/lzma.h"
#include "ab/lua_chunk.h"
#include "ab/lua_data_json.h"
#include "ab/pack.h"
#include "ab/sprite.h"
#include "ab/text.h"
#include "ab/zip.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                              \
    do {                                                                               \
        if (!(expression)) {                                                           \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);           \
            return false;                                                              \
        }                                                                              \
    } while (0)

static void put16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8));
    }
}

static void put32be(uint8_t *bytes, uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(value >> ((3 - index) * 8));
    }
}

static void put16be(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static size_t dat_string(uint8_t *bytes, const char *text) {
    size_t length = strlen(text);
    put16be(bytes, (uint16_t)length);
    memcpy(bytes + 2, text, length);
    return length + 2;
}

static size_t sprite_fixture(uint8_t output[512]) {
    memset(output, 0, 512);
    memcpy(output, "KA3D", 4);
    memcpy(output + 8, "SPRT", 4);
    put16be(output + 16, 1);
    size_t offset = 18 + dat_string(output + 18, "atlas.pvr");
    put16be(output + offset, 2);
    offset += 2;
    const uint16_t fields[2][6] = {{0x8000, 0xffff, 0, 0, 0x7fff, 0xffcd},
                                   {1, 2, 30, 40, 15, 20}};
    for (size_t index = 0; index < 2; ++index) {
        offset += dat_string(output + offset, "same");
        for (size_t field = 0; field < 6; ++field) {
            put16be(output + offset, fields[index][field]);
            offset += 2;
        }
    }
    put32be(output + 4, (uint32_t)offset - 8);
    put32be(output + 12, (uint32_t)offset - 16);
    return offset;
}

static bool sprite_metadata_and_bounds(void) {
    uint8_t bytes[512];
    size_t size = sprite_fixture(bytes);
    AbError error = {0};
    AbSpriteSheet sheet = {0};
    CHECK(ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, &sheet, &error));
    CHECK(sheet.count == 2 && sheet.texture.size == 9);
    CHECK(sheet.texture.data == bytes + 20);
    CHECK(sheet.sprites[0].x == INT16_MIN && sheet.sprites[0].y == -1);
    CHECK(sheet.sprites[0].width == 0 && sheet.sprites[0].height == 0);
    CHECK(sheet.sprites[0].pivot_x == INT16_MAX && sheet.sprites[0].pivot_y == -51);
    CHECK(sheet.sprites[1].width == 30 && sheet.sprites[1].height == 40);
    const uint8_t name[] = "same";
    CHECK(ab_sprite_find(&sheet, (AbSpan){name, 4}) == sheet.sprites + 1);
    CHECK(!ab_sprite_find(&sheet, (AbSpan){name, 3}));
    ab_sprite_sheet_free(&sheet);
    CHECK(!sheet.sprites && !sheet.count && !sheet.texture.data);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 2 * sizeof(AbSpriteRegion) - 1,
                                &sheet, &error));
    for (size_t prefix = 0; prefix < size; ++prefix) {
        if (prefix >= 8) {
            put32be(bytes + 4, (uint32_t)prefix - 8);
        }
        if (prefix >= 16) {
            put32be(bytes + 12, (uint32_t)prefix - 16);
        }
        CHECK(!ab_sprite_sheet_read((AbSpan){bytes, prefix}, 1024, &sheet, &error));
        CHECK(!sheet.sprites && !sheet.texture.data);
    }
    size = sprite_fixture(bytes);
    bytes[17] = 2;
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, &sheet, &error));
    size = sprite_fixture(bytes);
    put16be(bytes + 29, 0xffff);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, &sheet, &error));
    put16be(bytes + 29, 0x7fff);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, SIZE_MAX, &sheet, &error));
    size = sprite_fixture(bytes);
    put16be(bytes + 18, 0x8000);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, &sheet, &error));
    size = sprite_fixture(bytes);
    put16be(bytes + 31, 0x7fff);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, &sheet, &error));
    size = sprite_fixture(bytes);
    ++bytes[4];
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, &sheet, &error));
    size = sprite_fixture(bytes);
    put32be(bytes + 4, (uint32_t)size - 7);
    put32be(bytes + 12, (uint32_t)size - 15);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size + 1}, 1024, &sheet, &error));
    size = sprite_fixture(bytes);
    memcpy(bytes + size, "SKIP", 4);
    put32be(bytes + size + 4, 1);
    bytes[size + 8] = 42;
    put32be(bytes + 4, (uint32_t)size + 1);
    CHECK(ab_sprite_sheet_read((AbSpan){bytes, size + 9}, 1024, &sheet, &error));
    ab_sprite_sheet_free(&sheet);
    memcpy(bytes + size, bytes + 8, size - 8);
    put32be(bytes + 4, (uint32_t)(2 * size) - 16);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, 2 * size - 8}, 1024, &sheet, &error));
    size = sprite_fixture(bytes);
    put16be(bytes + 29, 0);
    put32be(bytes + 4, 23);
    put32be(bytes + 12, 15);
    CHECK(ab_sprite_sheet_read((AbSpan){bytes, 31}, 0, &sheet, &error));
    CHECK(sheet.count == 0 && !sheet.sprites);
    ab_sprite_sheet_free(&sheet);
    memcpy(bytes + 8, "SKIP", 4);
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, 31}, 1024, &sheet, &error));
    AbDatReader reader;
    CHECK(!ab_dat_open((AbSpan){NULL, 16}, "KA3D", &reader, &error));
    CHECK(!ab_sprite_sheet_read((AbSpan){bytes, size}, 1024, NULL, &error));
    return true;
}

static size_t font_fixture(uint8_t output[512], uint16_t version) {
    memset(output, 0, 512);
    memcpy(output, "KA3D", 4);
    memcpy(output + 8, "FONT", 4);
    put16be(output + 16, version);
    size_t offset = 18 + dat_string(output + 18, "font.pvr");
    put16be(output + offset, 48);
    put16be(output + offset + 2, 0xffff);
    put16be(output + offset + 4, 2);
    offset += 6;
    const uint16_t fields[2][5] = {{2, 3, 30, 0xfffe, 25}, {4, 5, 20, 40, 30}};
    for (size_t index = 0; index < 2; ++index) {
        if (version == 1) {
            put16be(output + offset, index ? 65 : 0xff01);
            offset += 2;
        } else {
            put32be(output + offset, index ? 65 : 0x12345);
            offset += 4;
        }
        for (size_t field = 0; field < 5; ++field) {
            put16be(output + offset, fields[index][field]);
            offset += 2;
        }
    }
    put32be(output + 4, (uint32_t)offset - 8);
    put32be(output + 12, (uint32_t)offset - 16);
    return offset;
}

static bool font_metadata_and_bounds(void) {
    uint8_t bytes[512];
    AbError error = {0};
    AbBitmapFont font = {0};
    for (uint16_t version = 1; version <= 2; ++version) {
        size_t size = font_fixture(bytes, version);
        CHECK(ab_font_read((AbSpan){bytes, size}, 1024, &font, &error));
        CHECK(font.version == version && font.count == 2);
        CHECK(font.texture.size == 8 && font.texture.data == bytes + 20);
        CHECK(font.leading == 48 && font.tracking == -1);
        CHECK(font.glyphs[0].code == (version == 1 ? 0xff01u : 0x12345u));
        CHECK(font.glyphs[0].height == -2 && font.glyphs[0].pivot_y == 25);
        CHECK(font.max_ascending == 30 && font.max_descending == 10);
        CHECK(ab_font_glyph(&font, 65) == font.glyphs + 1);
        CHECK(!ab_font_glyph(&font, 32));
        ab_font_free(&font);
        CHECK(!font.glyphs && !font.count && !font.texture.data);
        CHECK(!ab_font_read((AbSpan){bytes, size}, 2 * sizeof(AbFontGlyph) - 1, &font,
                            &error));
        for (size_t prefix = 0; prefix < size; ++prefix) {
            if (prefix >= 8) {
                put32be(bytes + 4, (uint32_t)prefix - 8);
            }
            if (prefix >= 16) {
                put32be(bytes + 12, (uint32_t)prefix - 16);
            }
            CHECK(!ab_font_read((AbSpan){bytes, prefix}, 1024, &font, &error));
            CHECK(!font.glyphs && !font.texture.data);
        }
    }
    size_t size = font_fixture(bytes, 1);
    put16be(bytes + 16, 3);
    CHECK(!ab_font_read((AbSpan){bytes, size}, 1024, &font, &error));
    size = font_fixture(bytes, 1);
    put16be(bytes + 32, 0xffff);
    CHECK(!ab_font_read((AbSpan){bytes, size}, 1024, &font, &error));
    put16be(bytes + 32, 0x7fff);
    CHECK(!ab_font_read((AbSpan){bytes, size}, SIZE_MAX, &font, &error));
    size = font_fixture(bytes, 1);
    put16be(bytes + 18, 0x8000);
    CHECK(!ab_font_read((AbSpan){bytes, size}, 1024, &font, &error));
    size = font_fixture(bytes, 1);
    put32be(bytes + 4, (uint32_t)size - 7);
    put32be(bytes + 12, (uint32_t)size - 15);
    CHECK(!ab_font_read((AbSpan){bytes, size + 1}, 1024, &font, &error));
    size = font_fixture(bytes, 1);
    memcpy(bytes + size, bytes + 8, size - 8);
    put32be(bytes + 4, (uint32_t)(2 * size) - 16);
    CHECK(!ab_font_read((AbSpan){bytes, 2 * size - 8}, 1024, &font, &error));
    size = font_fixture(bytes, 1);
    memcpy(bytes + 8, "SKIP", 4);
    CHECK(!ab_font_read((AbSpan){bytes, size}, 1024, &font, &error));
    size = font_fixture(bytes, 1);
    put16be(bytes + 32, 0);
    put32be(bytes + 4, 26);
    put32be(bytes + 12, 18);
    CHECK(ab_font_read((AbSpan){bytes, 34}, 0, &font, &error));
    CHECK(font.count == 0 && font.max_ascending == 0 && font.max_descending == 0);
    ab_font_free(&font);
    CHECK(!ab_font_read((AbSpan){bytes, size}, 1024, NULL, &error));
    return true;
}

static size_t dat_chunk(uint8_t *output, const char *tag, AbSpan payload) {
    memcpy(output, tag, 4);
    put32be(output + 4, (uint32_t)payload.size);
    memcpy(output + 8, payload.data, payload.size);
    return payload.size + 8;
}

static size_t text_fixture(uint8_t output[512]) {
    memset(output, 0, 512);
    memcpy(output, "KA3D", 4);
    memcpy(output + 8, "TEXT", 4);
    put16be(output + 16, 1);
    uint8_t list[64];
    put16be(list, 2);
    size_t length = 2 + dat_string(list + 2, "en_US");
    length += dat_string(list + length, "ja_JP");
    size_t offset = 18 + dat_chunk(output + 18, "LDAT", (AbSpan){list, length});
    length = 2 + dat_string(list + 2, "label");
    length += dat_string(list + length, "blank");
    offset += dat_chunk(output + offset, "LIDS", (AbSpan){list, length});
    length = dat_string(list, "caf\xc3\xa9");
    length += dat_string(list + length, "");
    offset += dat_chunk(output + offset, "TXGP", (AbSpan){list, length});
    length = dat_string(list, "\xe7\xa9\xba");
    length += dat_string(list + length, " ");
    offset += dat_chunk(output + offset, "TXGP", (AbSpan){list, length});
    put32be(output + 4, (uint32_t)offset - 8);
    put32be(output + 12, (uint32_t)offset - 16);
    return offset;
}

static bool text_metadata_and_bounds(void) {
    uint8_t bytes[512];
    size_t size = text_fixture(bytes);
    AbError error = {0};
    AbTextCatalog catalog = {0};
    CHECK(ab_text_read((AbSpan){bytes, size}, 8 * sizeof(AbSpan), &catalog, &error));
    CHECK(catalog.locale_count == 2 && catalog.id_count == 2);
    CHECK(catalog.locales[0].data == bytes + 30);
    CHECK(catalog.translations[0].size == 5);
    CHECK(catalog.translations[1].size == 0);
    CHECK(catalog.translations[2].size == 3);
    CHECK(catalog.translations[3].size == 1);
    const uint8_t locale[] = "en_US";
    const uint8_t id[] = "blank";
    AbSpan result;
    CHECK(ab_text_find(&catalog, (AbSpan){locale, 5}, (AbSpan){id, 5}, &result));
    CHECK(result.size == 0 && result.data);
    CHECK(!ab_text_find(&catalog, (AbSpan){locale, 4}, (AbSpan){id, 5}, &result));
    CHECK(!result.size && !result.data);
    CHECK(!ab_text_find(&catalog, (AbSpan){locale, 5}, (AbSpan){id, 4}, &result));
    ab_text_free(&catalog);
    CHECK(!catalog.locales && !catalog.ids && !catalog.translations);
    CHECK(
        !ab_text_read((AbSpan){bytes, size}, 8 * sizeof(AbSpan) - 1, &catalog, &error));
    for (size_t prefix = 0; prefix < size; ++prefix) {
        if (prefix >= 8) {
            put32be(bytes + 4, (uint32_t)prefix - 8);
        }
        if (prefix >= 16) {
            put32be(bytes + 12, (uint32_t)prefix - 16);
        }
        CHECK(!ab_text_read((AbSpan){bytes, prefix}, 1024, &catalog, &error));
        CHECK(!catalog.locales && !catalog.ids && !catalog.translations);
    }
    size = text_fixture(bytes);
    put16be(bytes + 16, 2);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    put16be(bytes + 26, 0xffff);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    put16be(bytes + 26, 0x7fff);
    CHECK(!ab_text_read((AbSpan){bytes, size}, SIZE_MAX, &catalog, &error));
    size = text_fixture(bytes);
    put16be(bytes + 28, 0x8000);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    memcpy(bytes + 42, "TXGP", 4);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    memcpy(bytes + 66, "LIDS", 4);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    memcpy(bytes + 83, "SKIP", 4);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    put16be(bytes + 74, 4);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    put16be(bytes + 74, 0x7fff);
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    memcpy(bytes + size, bytes + 66, 17);
    put32be(bytes + 4, (uint32_t)size + 9);
    put32be(bytes + 12, (uint32_t)size + 1);
    CHECK(!ab_text_read((AbSpan){bytes, size + 17}, 1024, &catalog, &error));
    size = text_fixture(bytes);
    memcpy(bytes + size, "SKIP", 4);
    put32be(bytes + size + 4, 0);
    put32be(bytes + 4, (uint32_t)size);
    put32be(bytes + 12, (uint32_t)size - 8);
    CHECK(ab_text_read((AbSpan){bytes, size + 8}, 1024, &catalog, &error));
    ab_text_free(&catalog);
    CHECK(!ab_text_read((AbSpan){NULL, size}, 1024, &catalog, &error));
    CHECK(!ab_text_read((AbSpan){bytes, size}, 1024, NULL, &error));
    CHECK(!ab_text_find(NULL, (AbSpan){0}, (AbSpan){0}, &result));
    return true;
}

static size_t composite_fixture(uint8_t output[512], bool extended, uint16_t version) {
    memset(output, 0, 512);
    memcpy(output, extended ? "RVIO" : "KA3D", 4);
    memcpy(output + 8, "COMP", 4);
    put16be(output + 16, version);
    put16be(output + 18, 1);
    size_t offset = 20 + dat_string(output + 20, "layout");
    put16be(output + offset, 1);
    offset += 2;
    offset += dat_string(output + offset, "part");
    if (extended) {
        offset += dat_string(output + offset, "sprite");
    }
    put16be(output + offset, 0xfff9);
    put16be(output + offset + 2, 0x7fff);
    offset += 4;
    if (extended) {
        put32be(output + offset, 0x3f000000);
        put32be(output + offset + 4, 0x40000000);
        put32be(output + offset + 8, 0x42b40000);
        output[offset + 12] = 1;
        offset += 14;
    } else if (version == 2) {
        put16be(output + offset, 1);
        offset += 2;
        offset += dat_string(output + offset, "point");
        put16be(output + offset, 0x8000);
        put16be(output + offset + 2, 5);
        offset += 4;
    }
    put32be(output + 4, (uint32_t)offset - 8);
    put32be(output + 12, (uint32_t)offset - 16);
    return offset;
}

static bool composite_metadata_and_bounds(void) {
    uint8_t bytes[512];
    AbError error = {0};
    AbCompositeSet set = {0};
    for (unsigned variant = 0; variant < 3; ++variant) {
        bool extended = variant == 2;
        uint16_t version = variant == 1 ? 2 : 1;
        size_t size = composite_fixture(bytes, extended, version);
        size_t allocation = sizeof(AbComposite) + sizeof(AbCompositePart) +
                            (variant == 1 ? sizeof(AbCompositePoint) : 0);
        CHECK(ab_composite_read((AbSpan){bytes, size}, allocation, &set, &error));
        CHECK(set.count == 1 && set.version == version && set.extended == extended);
        CHECK(set.composites[0].name.data == bytes + 22);
        CHECK(set.composites[0].part_count == 1);
        const AbCompositePart *part = set.composites[0].parts;
        CHECK(part->sprite.size == 4 && part->x == -7 && part->y == INT16_MAX);
        if (extended) {
            CHECK(part->tag.size == 6 && part->flip_x && !part->flip_y);
            CHECK(part->scale_x_bits == 0x3f000000);
            CHECK(part->scale_y_bits == 0x40000000);
            CHECK(part->rotation_bits == 0x42b40000);
        } else {
            CHECK(!part->tag.size && !part->tag.data);
            CHECK(part->scale_x_bits == 0x3f800000);
            CHECK(part->scale_y_bits == 0x3f800000);
            CHECK(!part->rotation_bits && !part->flip_x && !part->flip_y);
        }
        if (variant == 1) {
            CHECK(set.composites[0].point_count == 1);
            CHECK(set.composites[0].points[0].x == INT16_MIN);
            CHECK(set.composites[0].points[0].y == 5);
        }
        ab_composite_free(&set);
        CHECK(!set.composites && !set.count);
        CHECK(!ab_composite_read((AbSpan){bytes, size}, allocation - 1, &set, &error));
        CHECK(!ab_composite_read((AbSpan){bytes, size}, sizeof(AbComposite) - 1, &set,
                                 &error));
        for (size_t prefix = 0; prefix < size; ++prefix) {
            if (prefix >= 8) {
                put32be(bytes + 4, (uint32_t)prefix - 8);
            }
            if (prefix >= 16) {
                put32be(bytes + 12, (uint32_t)prefix - 16);
            }
            CHECK(!ab_composite_read((AbSpan){bytes, prefix}, 4096, &set, &error));
            CHECK(!set.composites && !set.count);
        }
    }
    size_t size = composite_fixture(bytes, true, 1);
    bytes[60] = 128;
    CHECK(ab_composite_read((AbSpan){bytes, size}, 4096, &set, &error));
    CHECK(set.composites[0].parts[0].flip_x);
    ab_composite_free(&set);
    put16be(bytes + 16, 2);
    CHECK(!ab_composite_read((AbSpan){bytes, size}, 4096, &set, &error));
    size = composite_fixture(bytes, false, 2);
    put16be(bytes + 18, 0xffff);
    CHECK(!ab_composite_read((AbSpan){bytes, size}, 4096, &set, &error));
    put16be(bytes + 18, 0x7fff);
    CHECK(!ab_composite_read((AbSpan){bytes, size}, SIZE_MAX, &set, &error));
    size = composite_fixture(bytes, false, 2);
    put16be(bytes + 28, 0xffff);
    CHECK(!ab_composite_read((AbSpan){bytes, size}, 4096, &set, &error));
    size = composite_fixture(bytes, false, 2);
    put16be(bytes + 30, 0x8000);
    CHECK(!ab_composite_read((AbSpan){bytes, size}, 4096, &set, &error));
    size = composite_fixture(bytes, false, 2);
    put16be(bytes + 40, 0x7fff);
    CHECK(!ab_composite_read((AbSpan){bytes, size}, 4096, &set, &error));
    size = composite_fixture(bytes, false, 2);
    put32be(bytes + 4, (uint32_t)size - 7);
    put32be(bytes + 12, (uint32_t)size - 15);
    CHECK(!ab_composite_read((AbSpan){bytes, size + 1}, 4096, &set, &error));
    size = composite_fixture(bytes, false, 2);
    memcpy(bytes + size, bytes + 8, size - 8);
    put32be(bytes + 4, (uint32_t)(2 * size) - 16);
    CHECK(!ab_composite_read((AbSpan){bytes, 2 * size - 8}, 4096, &set, &error));
    size = composite_fixture(bytes, false, 2);
    memcpy(bytes + size, "SKIP", 4);
    put32be(bytes + size + 4, 0);
    put32be(bytes + 4, (uint32_t)size);
    CHECK(ab_composite_read((AbSpan){bytes, size + 8}, 4096, &set, &error));
    ab_composite_free(&set);
    size = composite_fixture(bytes, false, 1);
    put16be(bytes + 18, 0);
    put32be(bytes + 4, 12);
    put32be(bytes + 12, 4);
    CHECK(ab_composite_read((AbSpan){bytes, 20}, 0, &set, &error));
    CHECK(set.count == 0 && !set.composites);
    ab_composite_free(&set);
    memcpy(bytes + 8, "SKIP", 4);
    CHECK(!ab_composite_read((AbSpan){bytes, 20}, 4096, &set, &error));
    CHECK(!ab_composite_read((AbSpan){NULL, size}, 4096, &set, &error));
    CHECK(!ab_composite_read((AbSpan){bytes, size}, 4096, NULL, &error));
    return true;
}

static size_t png_chunk(uint8_t *output, const char *type, AbSpan content) {
    put32be(output, (uint32_t)content.size);
    memcpy(output + 4, type, 4);
    if (content.size) {
        memcpy(output + 8, content.data, content.size);
    }
    put32be(output + 8 + content.size,
            ab_crc32((AbSpan){output + 4, content.size + 4}));
    return content.size + 12;
}

static size_t png_fixture(uint8_t output[512], unsigned color, uint32_t width,
                          uint32_t height, AbSpan rows) {
    const uint8_t signature[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
    memcpy(output, signature, sizeof(signature));
    uint8_t header[13] = {0};
    put32be(header, width);
    put32be(header + 4, height);
    header[8] = 8;
    header[9] = (uint8_t)color;
    size_t offset = 8 + png_chunk(output + 8, "IHDR", (AbSpan){header, sizeof(header)});
    if (color == 3) {
        const uint8_t palette[6] = {10, 20, 30, 40, 50, 60};
        const uint8_t transparency[2] = {255, 64};
        offset += png_chunk(output + offset, "PLTE", (AbSpan){palette, 6});
        offset += png_chunk(output + offset, "tRNS", (AbSpan){transparency, 2});
    }
    uint8_t compressed[128] = {0x78, 1, 1};
    put16(compressed + 3, (uint16_t)rows.size);
    put16(compressed + 5, (uint16_t)(~(uint16_t)rows.size));
    memcpy(compressed + 7, rows.data, rows.size);
    uint32_t first = 1;
    uint32_t second = 0;
    for (size_t index = 0; index < rows.size; ++index) {
        first = (first + rows.data[index]) % 65521;
        second = (second + first) % 65521;
    }
    put32be(compressed + 7 + rows.size, second << 16 | first);
    offset += png_chunk(output + offset, "IDAT", (AbSpan){compressed, rows.size + 11});
    offset += png_chunk(output + offset, "IEND", (AbSpan){0});
    return offset;
}

static bool png_pixels_and_bounds(void) {
    const uint8_t rgba[16] = {10,  20,  30,  255, 40,  60,  80,  128,
                              100, 120, 140, 64,  160, 180, 200, 0};
    const uint8_t rows[5][18] = {
        {0, 10, 20, 30, 255, 40, 60, 80, 128, 0, 100, 120, 140, 64, 160, 180, 200, 0},
        {1, 10, 20, 30, 255, 30, 40, 50, 129, 1, 100, 120, 140, 64, 60, 60, 60, 192},
        {2, 10, 20, 30, 255, 40, 60, 80, 128, 2, 90, 100, 110, 65, 120, 120, 120, 128},
        {3, 10, 20, 30, 255, 35, 50, 65, 1, 3, 95, 110, 125, 193, 90, 90, 90, 160},
        {4, 10, 20, 30, 255, 30, 40, 50, 129, 4, 90, 100, 110, 65, 60, 60, 60, 192},
    };
    uint8_t bytes[512];
    AbImage image = {0};
    AbError error = {0};
    for (size_t filter = 0; filter < 5; ++filter) {
        size_t size = png_fixture(bytes, 6, 2, 2, (AbSpan){rows[filter], 18});
        AbPngInfo info;
        CHECK(ab_png_read_info((AbSpan){bytes, size}, &info, &error) &&
              info.width == 2 && info.height == 2 && info.color_type == 6 &&
              !info.has_transparency);
        CHECK(ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
        CHECK(image.width == 2 && image.height == 2 && image.rgba.size == 16);
        CHECK(memcmp(image.rgba.data, rgba, sizeof(rgba)) == 0);
        ab_image_free(&image);
        for (size_t prefix = 0; prefix < size; ++prefix) {
            info = (AbPngInfo){.width = 99};
            CHECK(!ab_png_read_info((AbSpan){bytes, prefix}, &info, &error) &&
                  info.width == 99);
            CHECK(!ab_png_decode((AbSpan){bytes, prefix}, 4096, &image, &error));
            CHECK(!image.rgba.data);
        }
        CHECK(!ab_png_decode((AbSpan){bytes, size}, 15, &image, &error));
        bytes[size - 1] ^= 1;
        CHECK(!ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
    }
    const uint8_t indexed[] = {0, 0, 1};
    size_t size = png_fixture(bytes, 3, 2, 1, (AbSpan){indexed, sizeof(indexed)});
    AbPngInfo info;
    CHECK(ab_png_read_info((AbSpan){bytes, size}, &info, &error) &&
          info.color_type == 3 && info.has_transparency);
    CHECK(ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
    const uint8_t palette_rgba[] = {10, 20, 30, 255, 40, 50, 60, 64};
    CHECK(memcmp(image.rgba.data, palette_rgba, sizeof(palette_rgba)) == 0);
    ab_image_free(&image);
    const uint8_t gray_alpha[] = {0, 17, 128};
    size = png_fixture(bytes, 4, 1, 1, (AbSpan){gray_alpha, sizeof(gray_alpha)});
    CHECK(ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){17, 17, 17, 128}, 4) == 0);
    ab_image_free(&image);
    const uint8_t rgb[] = {0, 1, 2, 3};
    size = png_fixture(bytes, 2, 1, 1, (AbSpan){rgb, sizeof(rgb)});
    CHECK(ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){1, 2, 3, 255}, 4) == 0);
    ab_image_free(&image);
    const uint8_t bad_palette[] = {0, 2};
    size = png_fixture(bytes, 3, 1, 1, (AbSpan){bad_palette, sizeof(bad_palette)});
    CHECK(!ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
    const uint8_t bad_filter[] = {5, 1, 2, 3, 4};
    size = png_fixture(bytes, 6, 1, 1, (AbSpan){bad_filter, sizeof(bad_filter)});
    CHECK(!ab_png_decode((AbSpan){bytes, size}, 4096, &image, &error));
    return true;
}

static size_t pvr_fixture(uint8_t bytes[128], uint64_t format, uint32_t width,
                          uint32_t height, AbSpan pixels) {
    memset(bytes, 0, 128);
    put32(bytes, 0x03525650);
    put32(bytes + 8, (uint32_t)format);
    put32(bytes + 12, (uint32_t)(format >> 32));
    put32(bytes + 20, format == 6 || format == UINT64_C(0x0808080861626772) ? 0 : 4);
    put32(bytes + 24, height);
    put32(bytes + 28, width);
    put32(bytes + 32, 1);
    put32(bytes + 36, 1);
    put32(bytes + 40, 1);
    put32(bytes + 44, 1);
    memcpy(bytes + 52, pixels.data, pixels.size);
    return 52 + pixels.size;
}

static bool pvr_pixels_and_bounds(void) {
    uint8_t bytes[128];
    const uint8_t packed4444[2] = {0x34, 0x12};
    size_t size =
        pvr_fixture(bytes, UINT64_C(0x0404040461626772), 1, 1, (AbSpan){packed4444, 2});
    AbImage image = {0};
    AbError error = {0};
    bool alpha = false;
    CHECK(ab_pvr_has_alpha((AbSpan){bytes, size}, &alpha, &error) && alpha);
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(image.width == 1 && image.height == 1 && image.rgba.size == 4);
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){17, 34, 51, 68}, 4) == 0);
    ab_image_free(&image);
    for (size_t prefix = 0; prefix < size; ++prefix) {
        CHECK(!ab_pvr_has_alpha((AbSpan){bytes, prefix}, &alpha, &error) && alpha);
        CHECK(!ab_pvr_decode((AbSpan){bytes, prefix}, 4096, &image, &error));
        CHECK(!image.rgba.data);
    }
    CHECK(!ab_pvr_decode((AbSpan){bytes, size}, 3, &image, &error));
    put32(bytes + 48, 1);
    CHECK(!ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    const uint8_t packed565[2] = {0x29, 0x8c};
    size =
        pvr_fixture(bytes, UINT64_C(0x0005060500626772), 1, 1, (AbSpan){packed565, 2});
    CHECK(ab_pvr_has_alpha((AbSpan){bytes, size}, &alpha, &error) && !alpha);
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){140, 134, 74, 255}, 4) == 0);
    ab_image_free(&image);
    const uint8_t rounding565[2] = {0x1a, 0x66};
    size = pvr_fixture(bytes, UINT64_C(0x0005060500626772), 1, 1,
                       (AbSpan){rounding565, 2});
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){99, 194, 214, 255}, 4) == 0);
    ab_image_free(&image);
    const uint8_t rgba[4] = {1, 2, 3, 4};
    size = pvr_fixture(bytes, UINT64_C(0x0808080861626772), 1, 1, (AbSpan){rgba, 4});
    CHECK(ab_pvr_has_alpha((AbSpan){bytes, size}, &alpha, &error) && alpha);
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, rgba, sizeof(rgba)) == 0);
    ab_image_free(&image);
    uint8_t block[8] = {0xf0, 0x0f, 0, 0, 0, 0, 0, 0};
    size = pvr_fixture(bytes, 6, 4, 4, (AbSpan){block, 8});
    CHECK(ab_pvr_has_alpha((AbSpan){bytes, size}, &alpha, &error) && !alpha);
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){255, 2, 2, 255}, 4) == 0);
    CHECK(memcmp(image.rgba.data + 8, (uint8_t[4]){2, 255, 2, 255}, 4) == 0);
    ab_image_free(&image);
    block[3] = 1;
    block[5] = 1;
    size = pvr_fixture(bytes, 6, 3, 3, (AbSpan){block, 8});
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){253, 0, 0, 255}, 4) == 0);
    CHECK(memcmp(image.rgba.data + 24, (uint8_t[4]){2, 255, 2, 255}, 4) == 0);
    ab_image_free(&image);
    const uint8_t differential[] = {0x20, 0x40, 0x60, 2, 0, 0, 0, 0};
    size = pvr_fixture(bytes, 6, 4, 4, (AbSpan){differential, 8});
    CHECK(ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){35, 68, 101, 255}, 4) == 0);
    ab_image_free(&image);
    bytes[52] = 4;
    CHECK(!ab_pvr_decode((AbSpan){bytes, size}, 4096, &image, &error));
    memset(bytes, 0, sizeof(bytes));
    put32(bytes, 52);
    put32(bytes + 4, 1);
    put32(bytes + 8, 1);
    put32(bytes + 16, 0x8010);
    put32(bytes + 20, 2);
    put32(bytes + 24, 16);
    put32(bytes + 28, 0xf000);
    put32(bytes + 32, 0x0f00);
    put32(bytes + 36, 0x00f0);
    put32(bytes + 40, 0x000f);
    put32(bytes + 44, 0x21525650);
    put32(bytes + 48, 1);
    memcpy(bytes + 52, packed4444, 2);
    CHECK(ab_pvr_decode((AbSpan){bytes, 54}, 4096, &image, &error));
    CHECK(memcmp(image.rgba.data, (uint8_t[4]){17, 34, 51, 68}, 4) == 0);
    ab_image_free(&image);
    put32(bytes + 28, 0x00ff);
    CHECK(!ab_pvr_decode((AbSpan){bytes, 54}, 4096, &image, &error));
    return true;
}

static void decode_hex(const char *text, uint8_t *output) {
    const char *digits = "0123456789abcdef";
    size_t length = strlen(text) / 2;
    for (size_t index = 0; index < length; ++index) {
        size_t high = (size_t)(strchr(digits, text[index * 2]) - digits);
        size_t low = (size_t)(strchr(digits, text[index * 2 + 1]) - digits);
        output[index] = (uint8_t)(high * 16 + low);
    }
}

static bool aes_vectors(void) {
    const char *ciphertexts[] = {
        "69c4e0d86a7b0430d8cdb78070b4c55a",
        "dda97ca4864cdfe06eaf70a0ec0d7191",
        "8ea2b7ca516745bfeafc49904b496089",
    };
    uint8_t key[32];
    uint8_t plaintext[32];
    uint8_t encrypted[32];
    uint8_t output[32];
    uint8_t iv[16] = {0};
    AbError error = {0};
    decode_hex("00112233445566778899aabbccddeeff", plaintext);
    for (size_t index = 0; index < sizeof(key); ++index) {
        key[index] = (uint8_t)index;
    }
    for (size_t variant = 0; variant < 3; ++variant) {
        decode_hex(ciphertexts[variant], encrypted);
        CHECK(ab_aes_cbc_decrypt((AbSpan){encrypted, 16},
                                 (AbSpan){key, 16 + variant * 8}, iv, output, &error));
        CHECK(memcmp(output, plaintext, 16) == 0);
        CHECK(ab_aes_cbc_encrypt((AbSpan){plaintext, 16},
                                 (AbSpan){key, 16 + variant * 8}, iv, output, &error));
        CHECK(memcmp(output, encrypted, 16) == 0);
    }
    decode_hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", key);
    decode_hex("000102030405060708090a0b0c0d0e0f", iv);
    decode_hex("f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d",
               encrypted);
    decode_hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51",
               plaintext);
    CHECK(ab_aes_cbc_decrypt((AbSpan){encrypted, 32}, (AbSpan){key, 32}, iv, encrypted,
                             &error));
    CHECK(memcmp(encrypted, plaintext, 32) == 0);
    CHECK(ab_aes_cbc_encrypt((AbSpan){encrypted, 32}, (AbSpan){key, 32}, iv, encrypted,
                             &error));
    decode_hex("f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d",
               output);
    CHECK(memcmp(encrypted, output, 32) == 0);
    CHECK(!ab_aes_cbc_encrypt((AbSpan){plaintext, 31}, (AbSpan){key, 32}, iv, output,
                              &error));
    CHECK(!ab_aes_cbc_encrypt((AbSpan){plaintext, 32}, (AbSpan){key, 17}, iv, output,
                              &error));
    CHECK(!ab_aes_cbc_decrypt((AbSpan){encrypted, 31}, (AbSpan){key, 32}, iv, output,
                              &error));
    CHECK(!ab_aes_cbc_decrypt((AbSpan){encrypted, 32}, (AbSpan){key, 17}, iv, output,
                              &error));
    CHECK(
        !ab_aes_cbc_decrypt((AbSpan){NULL, 16}, (AbSpan){key, 32}, iv, output, &error));
    memset(key, 0, sizeof(key));
    decode_hex("66e94bd4ef8a2c3b884cfa59ca342b2e", encrypted);
    AbBuffer asset = {0};
    CHECK(ab_asset_decrypt((AbSpan){encrypted, 16}, (AbSpan){key, 16}, &asset, &error));
    CHECK(asset.size == 16 && memcmp(asset.data, (uint8_t[16]){0}, 16) == 0);
    ab_buffer_free(&asset);
    CHECK(
        !ab_asset_decrypt((AbSpan){encrypted, 15}, (AbSpan){key, 16}, &asset, &error));
    CHECK(!asset.data);
    for (size_t length = 0; length <= sizeof(plaintext); ++length) {
        AbBuffer cipher = {0};
        CHECK(ab_asset_encrypt((AbSpan){plaintext, length}, (AbSpan){key, 32}, &cipher,
                               &error));
        CHECK(cipher.size == length + 16 - length % 16);
        CHECK(ab_asset_decrypt((AbSpan){cipher.data, cipher.size}, (AbSpan){key, 32},
                               &asset, &error));
        CHECK(asset.size == length && memcmp(asset.data, plaintext, length) == 0);
        ab_buffer_free(&asset);
        ab_buffer_free(&cipher);
    }
    CHECK(
        !ab_asset_encrypt((AbSpan){NULL, SIZE_MAX}, (AbSpan){key, 32}, &asset, &error));
    CHECK(!asset.data);
    return true;
}

static bool lzma_formats(void) {
    const char *vectors[] = {
        "894c5a4d410d0a1a0a5d0000010000000000000000000083fffbffffc0000000",
        "894c5a4d410d0a1a0a5d00000100030000000000000000309888a44a8e9ffff6638000",
        ("894c5a4d410d0a1a0a5d0000010040010000000000000020908476c9740b87d68d07fffffa4b8"
         "0"
         "00"),
    };
    const size_t sizes[] = {0, 3, 320};
    uint8_t input[128];
    AbError error = {0};
    for (size_t vector = 0; vector < 3; ++vector) {
        size_t input_size = strlen(vectors[vector]) / 2;
        decode_hex(vectors[vector], input);
        AbBuffer output = {0};
        CHECK(ab_lzma_asset((AbSpan){input, input_size}, 65536, &output, &error));
        CHECK(output.size == sizes[vector]);
        if (vector == 1) {
            CHECK(memcmp(output.data, "abc", 3) == 0);
        } else if (vector == 2) {
            for (size_t index = 0; index < output.size; ++index) {
                CHECK(output.data[index] == (uint8_t)"ABCD"[index % 4]);
            }
        }
        ab_buffer_free(&output);
        for (size_t prefix = 0; prefix < input_size; ++prefix) {
            CHECK(!ab_lzma_asset((AbSpan){input, prefix}, 65536, &output, &error));
            CHECK(!output.data);
        }
        input[input_size] = 0;
        CHECK(!ab_lzma_asset((AbSpan){input, input_size + 1}, 65536, &output, &error));
        input[9] = 255;
        CHECK(!ab_lzma_asset((AbSpan){input, input_size}, 65536, &output, &error));
        decode_hex(vectors[vector], input);
        put32(input + 18, 1);
        CHECK(!ab_lzma_asset((AbSpan){input, input_size}, 65536, &output, &error));
        decode_hex(vectors[vector], input);
        put32(input + 10, 65537);
        CHECK(!ab_lzma_asset((AbSpan){input, input_size}, 65536, &output, &error));
    }
    return true;
}

static void refresh_sevenzip_crc(uint8_t *bytes, size_t size) {
    size_t header_offset = 32 + ab_u32le(bytes + 12);
    put32(bytes + 28, ab_crc32((AbSpan){bytes + header_offset, size - header_offset}));
    put32(bytes + 8, ab_crc32((AbSpan){bytes + 12, 20}));
}

static size_t sevenzip_fixture(uint8_t bytes[256]) {
    memset(bytes, 0, 256);
    const uint8_t magic[] = {'7', 'z', 0xbc, 0xaf, 0x27, 0x1c, 0, 3};
    memcpy(bytes, magic, sizeof(magic));
    const char *packed_hex = "00309888a44a8e9ffff6638000";
    size_t packed_size = strlen(packed_hex) / 2;
    decode_hex(packed_hex, bytes + 32);
    put32(bytes + 12, (uint32_t)packed_size);
    size_t header_offset = 32 + packed_size;
    const uint8_t prefix[] = {
        1, 4, 6,  0, 1,    9, (uint8_t)packed_size,
        0, 7, 11, 1, 0,    1, 0x23,
        3, 1, 1,  5, 0x5d, 0, 0,
        1, 0, 12, 3, 0,    8, 10,
        1,
    };
    memcpy(bytes + header_offset, prefix, sizeof(prefix));
    size_t offset = header_offset + sizeof(prefix);
    put32(bytes + offset, ab_crc32((AbSpan){(const uint8_t *)"abc", 3}));
    offset += 4;
    const uint8_t suffix[] = {0, 0, 5, 1, 17, 5, 0, 'a', 0, 0, 0, 0, 0};
    memcpy(bytes + offset, suffix, sizeof(suffix));
    offset += sizeof(suffix);
    put32(bytes + 20, (uint32_t)(offset - header_offset));
    refresh_sevenzip_crc(bytes, offset);
    return offset;
}

static bool sevenzip_bounds(void) {
    uint8_t bytes[256];
    size_t size = sevenzip_fixture(bytes);
    AbBuffer output = {0};
    AbError error = {0};
    CHECK(ab_asset_unpack((AbSpan){bytes, size}, 65536, &output, &error));
    CHECK(output.size == 3 && memcmp(output.data, "abc", 3) == 0);
    ab_buffer_free(&output);
    for (size_t prefix = 0; prefix < size; ++prefix) {
        CHECK(!ab_asset_unpack((AbSpan){bytes, prefix}, 65536, &output, &error));
        CHECK(!output.data);
    }
    bytes[8] ^= 1;
    CHECK(!ab_asset_unpack((AbSpan){bytes, size}, 65536, &output, &error));
    size = sevenzip_fixture(bytes);
    bytes[size - 1] ^= 1;
    CHECK(!ab_asset_unpack((AbSpan){bytes, size}, 65536, &output, &error));
    size = sevenzip_fixture(bytes);
    bytes[40] ^= 1;
    CHECK(!ab_asset_unpack((AbSpan){bytes, size}, 65536, &output, &error));
    size = sevenzip_fixture(bytes);
    bytes[32 + ab_u32le(bytes + 12) + 16] = 2;
    refresh_sevenzip_crc(bytes, size);
    CHECK(!ab_asset_unpack((AbSpan){bytes, size}, 65536, &output, &error));
    CHECK(strstr(error.message, "Unsupported") != NULL);
    uint8_t seed[256];
    size = sevenzip_fixture(seed);
    size_t header_offset = 32 + ab_u32le(seed + 12);
    for (size_t offset = header_offset; offset < size; ++offset) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            memcpy(bytes, seed, size);
            bytes[offset] ^= (uint8_t)(1u << bit);
            refresh_sevenzip_crc(bytes, size);
            if (ab_asset_unpack((AbSpan){bytes, size}, 65536, &output, &error)) {
                CHECK(output.size == 3 && memcmp(output.data, "abc", 3) == 0);
            }
            ab_buffer_free(&output);
        }
    }
    return true;
}

static bool hashes(void) {
    static const struct {
        const char *input;
        const char *digest;
    } vectors[] = {
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
    };
    AbError error = {0};
    for (size_t index = 0; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        uint8_t digest[32];
        char hex[65];
        AbSpan input = {(const uint8_t *)vectors[index].input,
                        strlen(vectors[index].input)};
        CHECK(ab_sha256(input, digest, &error));
        ab_digest_hex(digest, hex);
        CHECK(strcmp(hex, vectors[index].digest) == 0);
    }
    CHECK(ab_crc32((AbSpan){(const uint8_t *)"123456789", 9}) == UINT32_C(0xcbf43926));
    CHECK(ab_crc32((AbSpan){0}) == 0);
    CHECK(!ab_sha256((AbSpan){NULL, 1}, (uint8_t[32]){0}, &error));
    CHECK(ab_range(SIZE_MAX, SIZE_MAX, 0));
    CHECK(!ab_range(SIZE_MAX, SIZE_MAX - 1, 4));
    CHECK(!ab_slice((AbSpan){NULL, 1}, 0, 1, &(AbSpan){0}));
    return true;
}

static size_t lua_fixture(uint8_t bytes[128]) {
    memset(bytes, 0, 128);
    const uint8_t header[12] = {0x1b, 'L', 'u', 'a', 0x51, 0, 1, 4, 4, 4, 4, 0};
    memcpy(bytes, header, sizeof(header));
    bytes[26] = 2;
    bytes[27] = 2;
    put32(bytes + 28, 1);
    put32(bytes + 32, 0x0080001e);
    return 56;
}

static bool lua_chunk_bounds(void) {
    uint8_t bytes[128];
    size_t size = lua_fixture(bytes);
    AbLuaChunk chunk = {0};
    AbError error = {0};
    CHECK(ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    CHECK(chunk.function_count == 1 && chunk.instruction_count == 1);
    CHECK(chunk.root.stack_size == 2 && chunk.root.vararg_flags == 2);
    CHECK(chunk.root.constant_count == 0 && chunk.root.child_count == 0);
    ab_lua_close(&chunk);
    for (size_t prefix = 0; prefix < size; ++prefix) {
        CHECK(!ab_lua_read((AbSpan){bytes, prefix}, 4096, &chunk, &error));
        CHECK(!chunk.root.constants && !chunk.root.children && !chunk.input.data);
    }
    CHECK(!ab_lua_read((AbSpan){bytes, size + 1}, 4096, &chunk, &error));
    bytes[10] = 8;
    CHECK(!ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    size = lua_fixture(bytes);
    put32(bytes + 28, UINT32_MAX);
    CHECK(!ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    size = lua_fixture(bytes);
    bytes[32] = 63;
    CHECK(!ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    size = lua_fixture(bytes);
    put32(bytes + 32, 1);
    CHECK(!ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    size = lua_fixture(bytes);
    put32(bytes + 36, 1);
    bytes[40] = AB_LUA_NUMBER;
    put32(bytes + 41, 0x3f800000);
    memset(bytes + 45, 0, 20);
    size = 61;
    CHECK(!ab_lua_read((AbSpan){bytes, size}, 0, &chunk, &error));
    CHECK(ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    CHECK(chunk.root.constants[0].kind == AB_LUA_NUMBER);
    CHECK(chunk.root.constants[0].bits == 0x3f800000);
    ab_lua_close(&chunk);
    bytes[40] = 9;
    CHECK(!ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    return true;
}

static bool lua_data_constructors(void) {
    uint8_t bytes[128];
    lua_fixture(bytes);
    const uint32_t words[] = {
        (UINT32_C(13) << 23) | (UINT32_C(20) << 14) | 10,
        UINT32_C(0x80000000) | (UINT32_C(257) << 14) | 9,
        (UINT32_C(2) << 14) | 7,
        UINT32_C(0x0080001e),
    };
    put32(bytes + 28, 4);
    for (size_t index = 0; index < 4; ++index) {
        put32(bytes + 32 + index * 4, words[index]);
    }
    put32(bytes + 48, 3);
    bytes[52] = AB_LUA_STRING;
    put32(bytes + 53, 2);
    memcpy(bytes + 57, "x", 2);
    bytes[59] = AB_LUA_NUMBER;
    put32(bytes + 60, 0x3fc00000);
    bytes[64] = AB_LUA_STRING;
    put32(bytes + 65, 6);
    memcpy(bytes + 69, "level", 6);
    memset(bytes + 75, 0, 16);
    size_t size = 91;
    AbLuaChunk chunk = {0};
    AbLuaData data = {0};
    AbError error = {0};
    CHECK(ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(data.table_count == 2 && data.globals->count == 1);
    AbDataValue level_key = {.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)"level", 5}};
    AbDataValue x_key = {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"x", 1}};
    const AbDataValue *level = ab_data_get(data.globals, level_key);
    CHECK(level && level->kind == AB_DATA_TABLE);
    const AbDataTableLayout *layout = ab_lua_table_layout(&data, level->table);
    CHECK(layout && layout->array_hint == 13 && layout->hash_capacity == 32 &&
          layout->no_nil_assignments);
    const AbDataValue *x = ab_data_get(level->table, x_key);
    CHECK(x && x->kind == AB_DATA_NUMBER && x->bits == 0x3fc00000);
    FILE *file = tmpfile();
    CHECK(file != NULL);
    CHECK(ab_lua_data_json(&data, file, &error));
    CHECK(fclose(file) == 0);
    level->table->entries[0].value =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = data.globals};
    file = tmpfile();
    CHECK(file != NULL);
    CHECK(!ab_lua_data_json(&data, file, &error));
    CHECK(strstr(error.message, "cycle") != NULL);
    CHECK(fclose(file) == 0);
    ab_lua_data_close(&data);
    CHECK(!ab_lua_data(&chunk, 0, &data, &error));
    ab_lua_data_close(&data);
    ab_lua_close(&chunk);
    put32(bytes + 32, 28);
    CHECK(ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(strstr(error.message, "behavioral recovery") != NULL);
    CHECK(!data.owned && !data.globals);
    ab_lua_close(&chunk);
    memmove(bytes + 52, bytes + 48, 43);
    put32(bytes + 28, 5);
    put32(bytes + 32, 10);
    put32(bytes + 36, (UINT32_C(1) << 6) | (UINT32_C(1) << 14) | 1);
    put32(bytes + 40, (UINT32_C(1) << 23) | (UINT32_C(1) << 14) | 34);
    put32(bytes + 44, (UINT32_C(2) << 14) | 7);
    put32(bytes + 48, UINT32_C(0x0080001e));
    size = 95;
    CHECK(ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    level = ab_data_get(data.globals, level_key);
    CHECK(level && level->kind == AB_DATA_TABLE);
    x = ab_data_get(level->table,
                    (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x3f800000});
    CHECK(x && x->bits == 0x3fc00000);
    ab_lua_data_close(&data);
    ab_lua_close(&chunk);
    memmove(bytes + 48, bytes + 44, size - 44);
    put32(bytes + 28, 6);
    put32(bytes + 40, (UINT32_C(1) << 23) | 34);
    put32(bytes + 44, 2);
    size += 4;
    CHECK(ab_lua_read((AbSpan){bytes, size}, 4096, &chunk, &error));
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    level = ab_data_get(data.globals, level_key);
    CHECK(level && level->kind == AB_DATA_TABLE);
    x = ab_data_get(level->table,
                    (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x424c0000});
    CHECK(x && x->bits == 0x3fc00000);
    ab_lua_data_close(&data);
    ab_lua_close(&chunk);
    return true;
}

static bool lua_literal_table_prefix(void) {
    uint8_t words[24];
    AbLuaConstant constant = {.kind = AB_LUA_STRING,
                              .string = {(const uint8_t *)"image-group", 11}};
    AbLuaChunk chunk = {.input = {words, sizeof(words)},
                        .root = {.instructions = {words, 20},
                                 .stack_size = 2,
                                 .constants = &constant,
                                 .constant_count = 1}};
    AbLuaData data = {0};
    AbError error = {0};
    put32(words, 10);
    put32(words + 4, (1u << 6) | 1u);
    put32(words + 8, (1u << 23) | (1u << 14) | 34u);
    put32(words + 12, 28); /* A call is outside the literal prefix. */
    put32(words + 16, (1u << 23) | 30u);
    CHECK(ab_lua_data_literal_table(&chunk, 3, 0, 4096, &data, &error));
    CHECK(!data.globals && data.returned.kind == AB_DATA_TABLE);
    CHECK(data.returned.table->count == 1);
    const AbDataValue *value = ab_data_get(
        data.returned.table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x3f800000});
    CHECK(value && value->kind == AB_DATA_STRING && value->string.size == 11);
    ab_lua_data_close(&data);
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(!ab_lua_data_literal_table(&chunk, 4, 0, 4096, &data, &error));
    CHECK(!data.owned && !data.returned.table && !data.memory_limit);
    CHECK(!ab_lua_data_literal_table(&chunk, 6, 0, 4096, &data, &error));
    CHECK(!ab_lua_data_literal_table(&chunk, 0, 0, 4096, &data, &error));
    CHECK(!ab_lua_data_literal_table(&chunk, 1, 1, 4096, &data, &error));
    CHECK(!ab_lua_data_literal_table(&chunk, 3, 1, 4096, &data, &error));
    CHECK(!ab_lua_data_literal_table(&chunk, 3, 0, 0, &data, &error));
    const unsigned forbidden[] = {5, 7, 12, 22, 30};
    for (size_t index = 0; index < sizeof(forbidden) / sizeof(*forbidden); ++index) {
        put32(words + 12, forbidden[index]);
        CHECK(!ab_lua_data_literal_table(&chunk, 4, 0, 4096, &data, &error));
        CHECK(!data.owned && !data.returned.table);
    }
    put32(words + 8, (1u << 23) | 34u);
    put32(words + 12, 1);
    CHECK(!ab_lua_data_literal_table(&chunk, 3, 0, 4096, &data, &error));
    CHECK(ab_lua_data_literal_table(&chunk, 4, 0, 4096, &data, &error));
    CHECK(data.returned.table->count == 1);
    ab_lua_data_close(&data);
    return true;
}

static bool lua_numeric_constructors(void) {
    uint8_t instructions[8];
    AbLuaConstant constants[2] = {{.kind = AB_LUA_NUMBER, .bits = 0x3fc00000},
                                  {.kind = AB_LUA_NUMBER, .bits = 0x40100000}};
    AbLuaChunk chunk = {.input = {instructions, sizeof(instructions)},
                        .root = {.instructions = {instructions, sizeof(instructions)},
                                 .stack_size = 2,
                                 .constants = constants,
                                 .constant_count = 2}};
    AbLuaData data = {0};
    AbError error = {0};
    put32(instructions + 4, (UINT32_C(2) << 23) | 30);
    const uint32_t expected[4] = {0x40700000, 0xbf400000, 0x40580000, 0x3f2aaaab};
    for (unsigned opcode = 12; opcode <= 15; ++opcode) {
        put32(instructions, opcode | (UINT32_C(256) << 23) | (UINT32_C(257) << 14));
        CHECK(ab_lua_data(&chunk, 4096, &data, &error));
        CHECK(data.returned.kind == AB_DATA_NUMBER &&
              data.returned.bits == expected[opcode - 12]);
        ab_lua_data_close(&data);
    }
    put32(instructions, 12 | (UINT32_C(256) << 23) | (UINT32_C(257) << 14));
    constants[0].bits = 0x3f800000;
    constants[1].bits = 0x33800000;
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(data.returned.bits == 0x3f800000);
    ab_lua_data_close(&data);
    put32(instructions, 14 | (UINT32_C(256) << 23) | (UINT32_C(257) << 14));
    constants[0].bits = 0x80000000;
    constants[1].bits = 0x40100000;
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(data.returned.bits == 0x80000000);
    ab_lua_data_close(&data);
    constants[0].kind = AB_LUA_STRING;
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(!data.owned);
    constants[0].kind = AB_LUA_NUMBER;
    constants[0].bits = 0x7f7fffff;
    constants[1].bits = 0x7f7fffff;
    put32(instructions, 12 | (UINT32_C(256) << 23) | (UINT32_C(257) << 14));
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(!data.owned);
    constants[1].bits = 0;
    put32(instructions, 15 | (UINT32_C(256) << 23) | (UINT32_C(257) << 14));
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    constants[1].bits = 0x80000000;
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    put32(instructions, 12 | (UINT32_C(256) << 23) | (UINT32_C(258) << 14));
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    put32(instructions, 12 | (UINT32_C(1) << 23) | (UINT32_C(257) << 14));
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    return true;
}

static bool lua_imported_constructors(void) {
    uint8_t instructions[32];
    AbLuaConstant constants[] = {
        {.kind = AB_LUA_STRING, .string = {(const uint8_t *)"inherited", 9}},
        {.kind = AB_LUA_STRING, .string = {(const uint8_t *)"object", 6}},
        {.kind = AB_LUA_STRING, .string = {(const uint8_t *)"result", 6}},
        {.kind = AB_LUA_STRING, .string = {(const uint8_t *)"x", 1}},
        {.kind = AB_LUA_NUMBER, .bits = 0x40100000},
    };
    AbDataValue inherited_key = {.kind = AB_DATA_STRING, .string = constants[0].string};
    AbDataValue object_key = {.kind = AB_DATA_STRING, .string = constants[1].string};
    AbDataValue result_key = {.kind = AB_DATA_STRING, .string = constants[2].string};
    AbDataEntry object_entry = {
        {.kind = AB_DATA_STRING, .string = constants[3].string},
        {.kind = AB_DATA_NUMBER, .bits = 0x3fc00000},
    };
    AbDataTable object = {.entries = &object_entry, .count = 1, .capacity = 1};
    AbDataEntry imports[] = {
        {inherited_key, {.kind = AB_DATA_NUMBER, .bits = 0x3fc00000}},
        {object_key, {.kind = AB_DATA_TABLE, .table = &object}},
    };
    AbDataTable globals = {.entries = imports, .count = 2, .capacity = 2};
    AbLuaChunk chunk = {.input = {instructions, sizeof(instructions)},
                        .root = {.instructions = {instructions, 20},
                                 .stack_size = 2,
                                 .constants = constants,
                                 .constant_count = 5}};
    AbLuaData data = {0};
    AbError error = {0};
    put32(instructions, 5);
    put32(instructions + 4, 7 | (UINT32_C(2) << 14));
    put32(instructions + 8, 5 | (UINT32_C(1) << 14));
    put32(instructions + 12, 6 | (UINT32_C(1) << 6) | (UINT32_C(259) << 14));
    put32(instructions + 16, 30 | (UINT32_C(1) << 6) | (UINT32_C(2) << 23));
    CHECK(ab_lua_data_with_globals(&chunk, 4096, &globals, &data, &error));
    CHECK(data.returned.kind == AB_DATA_NUMBER && data.returned.bits == 0x3fc00000);
    CHECK(data.globals != &globals && data.globals->count == 3);
    CHECK(ab_data_get(data.globals, object_key)->table == &object);
    CHECK(ab_data_get(data.globals, result_key)->bits == 0x3fc00000);
    ab_lua_data_close(&data);
    CHECK(globals.count == 2 && object.count == 1 &&
          object_entry.value.bits == 0x3fc00000);
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(!data.owned && !data.globals);
    CHECK(!ab_lua_data_with_globals(&chunk, sizeof(AbDataTable), &globals, &data,
                                    &error));
    CHECK(!data.owned && !data.globals);

    /* Local assignment shadows an imported binding without changing its owner. */
    chunk.root.instructions.size = 16;
    put32(instructions, 1 | (UINT32_C(4) << 14));
    put32(instructions + 4, 7);
    put32(instructions + 8, 5 | (UINT32_C(1) << 6));
    put32(instructions + 12, 30 | (UINT32_C(1) << 6) | (UINT32_C(2) << 23));
    CHECK(ab_lua_data_with_globals(&chunk, 4096, &globals, &data, &error));
    CHECK(data.returned.bits == 0x40100000 && imports[0].value.bits == 0x3fc00000);
    ab_lua_data_close(&data);
    put32(instructions, 3);
    CHECK(ab_lua_data_with_globals(&chunk, 4096, &globals, &data, &error));
    CHECK(data.returned.kind == AB_DATA_NIL &&
          !ab_data_get(data.globals, inherited_key));
    ab_lua_data_close(&data);
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(data.returned.kind == AB_DATA_NIL);
    ab_lua_data_close(&data);

    /* A later non-nil assignment removes the known-nil marker. */
    chunk.root.instructions.size = 24;
    put32(instructions + 8, 1 | (UINT32_C(4) << 14));
    put32(instructions + 12, 7);
    put32(instructions + 16, 5);
    put32(instructions + 20, 30 | (UINT32_C(2) << 23));
    CHECK(ab_lua_data_with_globals(&chunk, 4096, &globals, &data, &error));
    CHECK(data.returned.bits == 0x40100000);
    ab_lua_data_close(&data);

    chunk.root.instructions.size = 12;
    put32(instructions, 5 | (UINT32_C(1) << 14));
    put32(instructions + 4, 9 | (UINT32_C(259) << 23) | (UINT32_C(260) << 14));
    put32(instructions + 8, 30 | (UINT32_C(1) << 23));
    CHECK(!ab_lua_data_with_globals(&chunk, 4096, &globals, &data, &error));
    CHECK(strstr(error.message, "mutation of imported") != NULL && !data.owned);
    chunk.root.instructions.size = 16;
    put32(instructions + 4, 1 | (UINT32_C(1) << 6) | (UINT32_C(4) << 14));
    put32(instructions + 8, 34 | (UINT32_C(1) << 23) | (UINT32_C(1) << 14));
    put32(instructions + 12, 30 | (UINT32_C(1) << 23));
    CHECK(!ab_lua_data_with_globals(&chunk, 4096, &globals, &data, &error));
    CHECK(strstr(error.message, "mutation of imported") != NULL && !data.owned);
    CHECK(object_entry.value.bits == 0x3fc00000 && object.count == 1);
    return true;
}

static bool inflate_formats(void) {
    const uint8_t stored[] = {1, 3, 0, 0xfc, 0xff, 'a', 'b', 'c'};
    const uint8_t fixed[] = {0x73, 0x74, 0x72, 0x76, 0x71, 0x1c, 0xc5, 0x64, 0x63, 0};
    const uint8_t dynamic[] = {
        0xed, 0xca, 0x47, 0x01, 0x80, 0x30, 0x10, 0x45, 0x41, 0x2b, 0x5f, 0x01, 0x6a,
        0x62, 0x80, 0x92, 0xd0, 0xd9, 0x10, 0x08, 0x4d, 0x3d, 0x88, 0xe0, 0xf8, 0xce,
        0x33, 0xae, 0xf3, 0x5a, 0x73, 0x5f, 0x8f, 0xaa, 0x92, 0x9d, 0x8b, 0x82, 0x5d,
        0x1a, 0xf2, 0x1c, 0x37, 0xd9, 0xe1, 0x93, 0xf6, 0x8f, 0xa7, 0xf2, 0xb9, 0xd5,
        0x58, 0x5b, 0xc8, 0x91, 0xc9, 0x64, 0x32, 0x99, 0x4c, 0x26, 0x93, 0xc9, 0x64,
        0x32, 0x99, 0x4c, 0x26, 0x93, 0xff, 0xc8, 0x2f,
    };
    uint8_t output[4500];
    AbError error = {0};
    CHECK(ab_inflate_raw((AbSpan){stored, sizeof(stored)}, output, 3, &error));
    CHECK(memcmp(output, "abc", 3) == 0);
    CHECK(ab_inflate_raw((AbSpan){fixed, sizeof(fixed)}, output, 320, &error));
    for (size_t index = 0; index < 320; ++index) {
        CHECK(output[index] == (uint8_t)"ABCD"[index % 4]);
    }
    CHECK(ab_inflate_raw((AbSpan){dynamic, sizeof(dynamic)}, output, sizeof(output),
                         &error));
    const char phrase[] = "The quick brown fox jumps over the lazy dog. ";
    for (size_t index = 0; index < sizeof(output); ++index) {
        CHECK(output[index] == (uint8_t)phrase[index % (sizeof(phrase) - 1)]);
    }
    for (size_t prefix = 0; prefix < sizeof(dynamic); ++prefix) {
        CHECK(
            !ab_inflate_raw((AbSpan){dynamic, prefix}, output, sizeof(output), &error));
    }
    CHECK(!ab_inflate_raw((AbSpan){fixed, sizeof(fixed)}, output, 319, &error));
    CHECK(!ab_inflate_raw((AbSpan){fixed, sizeof(fixed)}, output, 321, &error));
    uint8_t trailing[sizeof(fixed) + 1];
    memcpy(trailing, fixed, sizeof(fixed));
    trailing[sizeof(fixed)] = 0;
    CHECK(!ab_inflate_raw((AbSpan){trailing, sizeof(trailing)}, output, 320, &error));
    const uint8_t reserved[] = {7};
    CHECK(!ab_inflate_raw((AbSpan){reserved, 1}, output, 0, &error));
    const uint8_t bad_stored[] = {1, 0, 0, 0, 0};
    CHECK(!ab_inflate_raw((AbSpan){bad_stored, sizeof(bad_stored)}, output, 0, &error));
    const uint8_t empty[] = {3, 0};
    CHECK(ab_inflate_raw((AbSpan){empty, 2}, NULL, 0, &error));
    const uint8_t multi[] = {0, 1, 0, 0xfe, 0xff, 'a', 1, 1, 0, 0xfe, 0xff, 'b'};
    CHECK(ab_inflate_raw((AbSpan){multi, sizeof(multi)}, output, 2, &error));
    CHECK(memcmp(output, "ab", 2) == 0);
    return true;
}

static size_t zip_fixture(uint8_t bytes[256], bool descriptor, bool signature) {
    memset(bytes, 0, 256);
    uint16_t flags = descriptor ? 8 : 0;
    uint32_t crc = ab_crc32((AbSpan){(const uint8_t *)"abc", 3});
    put32(bytes, 0x04034b50);
    put16(bytes + 4, 20);
    put16(bytes + 6, flags);
    if (!descriptor) {
        put32(bytes + 14, crc);
        put32(bytes + 18, 3);
        put32(bytes + 22, 3);
    }
    put16(bytes + 26, 1);
    bytes[30] = 'a';
    memcpy(bytes + 31, "abc", 3);
    size_t offset = 34;
    if (descriptor) {
        if (signature) {
            put32(bytes + offset, 0x08074b50);
            offset += 4;
        }
        put32(bytes + offset, crc);
        put32(bytes + offset + 4, 3);
        put32(bytes + offset + 8, 3);
        offset += 12;
    }
    size_t central = offset;
    put32(bytes + central, 0x02014b50);
    put16(bytes + central + 4, 20);
    put16(bytes + central + 6, 20);
    put16(bytes + central + 8, flags);
    put32(bytes + central + 16, crc);
    put32(bytes + central + 20, 3);
    put32(bytes + central + 24, 3);
    put16(bytes + central + 28, 1);
    bytes[central + 46] = 'a';
    offset += 47;
    put32(bytes + offset, 0x06054b50);
    put16(bytes + offset + 8, 1);
    put16(bytes + offset + 10, 1);
    put32(bytes + offset + 12, 47);
    put32(bytes + offset + 16, (uint32_t)central);
    return offset + 22;
}

static bool zip_bounds(void) {
    uint8_t bytes[256];
    AbError error = {0};
    for (unsigned mode = 0; mode < 3; ++mode) {
        size_t size = zip_fixture(bytes, mode > 0, mode == 2);
        AbZip zip = {0};
        AbBuffer extracted = {0};
        CHECK(ab_zip_open((AbSpan){bytes, size}, &zip, &error));
        CHECK(zip.count == 1);
        CHECK(ab_zip_find(&zip, "a") == zip.entries);
        CHECK(!ab_zip_find(&zip, "absent"));
        CHECK(!ab_zip_read(&zip, 0, 2, &extracted, &error));
        CHECK(!extracted.data);
        CHECK(ab_zip_read(&zip, 0, 3, &extracted, &error));
        CHECK(extracted.size == 3 && memcmp(extracted.data, "abc", 3) == 0);
        ab_buffer_free(&extracted);
        bytes[31] ^= 1;
        CHECK(!ab_zip_read(&zip, 0, 3, &extracted, &error));
        CHECK(!extracted.data);
        bytes[31] ^= 1;
        CHECK(!ab_zip_read(&zip, 1, 3, &extracted, &error));
        ab_zip_close(&zip);
        for (size_t prefix = 0; prefix < size; ++prefix) {
            CHECK(!ab_zip_open((AbSpan){bytes, prefix}, &zip, &error));
            CHECK(!zip.entries);
        }
        bytes[30] = 'z';
        CHECK(!ab_zip_open((AbSpan){bytes, size}, &zip, &error));
    }
    size_t size = zip_fixture(bytes, false, false);
    AbZip zip = {0};
    put32(bytes + size - 6, UINT32_MAX);
    CHECK(!ab_zip_open((AbSpan){bytes, size}, &zip, &error));
    size = zip_fixture(bytes, false, false);
    put16(bytes + 34 + 8, 1);
    CHECK(!ab_zip_open((AbSpan){bytes, size}, &zip, &error));
    size = zip_fixture(bytes, false, false);
    memcpy(bytes + 81, bytes + 34, 47);
    size_t end = 128;
    memset(bytes + end, 0, 22);
    put32(bytes + end, 0x06054b50);
    put16(bytes + end + 8, 2);
    put16(bytes + end + 10, 2);
    put32(bytes + end + 12, 94);
    put32(bytes + end + 16, 34);
    CHECK(!ab_zip_open((AbSpan){bytes, end + 22}, &zip, &error));
    CHECK(strstr(error.message, "duplicate") != NULL);
    return true;
}

typedef struct {
    uint32_t name;
    uint8_t type;
    uint32_t value;
    bool android;
} Attribute;

static size_t xml_start(uint8_t *bytes, uint32_t name, const Attribute *attributes,
                        size_t count) {
    size_t size = 36 + count * 20;
    memset(bytes, 0, size);
    put16(bytes, 0x102);
    put16(bytes + 2, 16);
    put32(bytes + 4, (uint32_t)size);
    put32(bytes + 12, UINT32_MAX);
    put32(bytes + 16, UINT32_MAX);
    put32(bytes + 20, name);
    put16(bytes + 24, 20);
    put16(bytes + 26, 20);
    put16(bytes + 28, (uint16_t)count);
    for (size_t index = 0; index < count; ++index) {
        uint8_t *attribute = bytes + 36 + index * 20;
        put32(attribute, attributes[index].android ? 0 : UINT32_MAX);
        put32(attribute + 4, attributes[index].name);
        put32(attribute + 8, UINT32_MAX);
        put16(attribute + 12, 8);
        attribute[15] = attributes[index].type;
        put32(attribute + 16, attributes[index].value);
    }
    return size;
}

static size_t xml_end(uint8_t *bytes, uint32_t name) {
    memset(bytes, 0, 24);
    put16(bytes, 0x103);
    put16(bytes + 2, 16);
    put32(bytes + 4, 24);
    put32(bytes + 12, UINT32_MAX);
    put32(bytes + 16, UINT32_MAX);
    put32(bytes + 20, name);
    return 24;
}

static size_t manifest_fixture(uint8_t bytes[4096], bool utf8) {
    const char *strings[] = {
        "http://schemas.android.com/apk/res/android",
        "manifest",
        "package",
        "org.example.test",
        "versionName",
        "1.2.3",
        "versionCode",
        "uses-sdk",
        "minSdkVersion",
        "targetSdkVersion",
        "application",
        "activity",
        "name",
        "ExampleActivity",
        "intent-filter",
        "action",
        "android.intent.action.MAIN",
        "category",
        "android.intent.category.LAUNCHER",
        "uses-feature",
        "glEsVersion",
    };
    size_t count = sizeof(strings) / sizeof(strings[0]);
    memset(bytes, 0, 4096);
    put16(bytes, 3);
    put16(bytes + 2, 8);
    uint8_t *pool = bytes + 8;
    put16(pool, 1);
    put16(pool + 2, 28);
    put32(pool + 8, (uint32_t)count);
    put32(pool + 16, utf8 ? 256 : 0);
    size_t start = 28 + count * 4;
    put32(pool + 20, (uint32_t)start);
    size_t offset = start;
    for (size_t index = 0; index < count; ++index) {
        size_t length = strlen(strings[index]);
        put32(pool + 28 + index * 4, (uint32_t)(offset - start));
        if (utf8) {
            pool[offset++] = (uint8_t)length;
            pool[offset++] = (uint8_t)length;
            memcpy(pool + offset, strings[index], length + 1);
            offset += length + 1;
        } else {
            put16(pool + offset, (uint16_t)length);
            offset += 2;
            for (size_t character = 0; character <= length; ++character) {
                put16(pool + offset, (uint8_t)strings[index][character]);
                offset += 2;
            }
        }
    }
    offset = (offset + 3) & ~(size_t)3;
    put32(pool + 4, (uint32_t)offset);
    offset += 8;
    const Attribute root[] = {{2, 3, 3, false}, {4, 3, 5, true}, {6, 0x10, 42, true}};
    offset += xml_start(bytes + offset, 1, root, 3);
    const Attribute sdk[] = {{8, 0x10, 9, true}, {9, 0x10, 22, true}};
    offset += xml_start(bytes + offset, 7, sdk, 2);
    offset += xml_end(bytes + offset, 7);
    const Attribute feature[] = {{20, 0x11, 0x20000, true}};
    offset += xml_start(bytes + offset, 19, feature, 1);
    offset += xml_end(bytes + offset, 19);
    offset += xml_start(bytes + offset, 10, NULL, 0);
    const Attribute activity[] = {{12, 3, 13, true}};
    offset += xml_start(bytes + offset, 11, activity, 1);
    offset += xml_start(bytes + offset, 14, NULL, 0);
    const Attribute action[] = {{12, 3, 16, true}};
    offset += xml_start(bytes + offset, 15, action, 1);
    offset += xml_end(bytes + offset, 15);
    const Attribute category[] = {{12, 3, 18, true}};
    offset += xml_start(bytes + offset, 17, category, 1);
    offset += xml_end(bytes + offset, 17);
    offset += xml_end(bytes + offset, 14);
    offset += xml_end(bytes + offset, 11);
    offset += xml_end(bytes + offset, 10);
    offset += xml_end(bytes + offset, 1);
    put32(bytes + 4, (uint32_t)offset);
    return offset;
}

static bool manifest_bounds(void) {
    uint8_t bytes[4096];
    AbManifest manifest;
    AbError error = {0};
    for (unsigned encoding = 0; encoding < 2; ++encoding) {
        size_t size = manifest_fixture(bytes, encoding != 0);
        CHECK(ab_manifest_read((AbSpan){bytes, size}, &manifest, &error));
        CHECK(strcmp(manifest.package, "org.example.test") == 0);
        CHECK(strcmp(manifest.version_name, "1.2.3") == 0);
        CHECK(strcmp(manifest.launcher, "ExampleActivity") == 0);
        CHECK(manifest.version_code == 42);
        CHECK(manifest.min_sdk == 9 && manifest.target_sdk == 22);
        CHECK(manifest.gles_version == 0x20000 && manifest.activity_count == 1);
        for (size_t prefix = 0; prefix < size; ++prefix) {
            if (prefix >= 8) {
                put32(bytes + 4, (uint32_t)prefix);
            }
            CHECK(!ab_manifest_read((AbSpan){bytes, prefix}, &manifest, &error));
        }
        put32(bytes + 4, (uint32_t)size);
        put32(bytes + size - 4, 2);
        CHECK(!ab_manifest_read((AbSpan){bytes, size}, &manifest, &error));
        size = manifest_fixture(bytes, encoding != 0);
        put32(bytes + 8 + 28, UINT32_MAX);
        CHECK(!ab_manifest_read((AbSpan){bytes, size}, &manifest, &error));
        size = manifest_fixture(bytes, encoding != 0);
        put32(bytes + 8 + 8, UINT32_MAX);
        CHECK(!ab_manifest_read((AbSpan){bytes, size}, &manifest, &error));
    }
    return true;
}

int main(void) {
    if (!hashes() || !png_pixels_and_bounds() || !pvr_pixels_and_bounds() ||
        !sprite_metadata_and_bounds() || !font_metadata_and_bounds() ||
        !text_metadata_and_bounds() || !composite_metadata_and_bounds() ||
        !lua_chunk_bounds() || !lua_data_constructors() ||
        !lua_literal_table_prefix() || !lua_numeric_constructors() ||
        !lua_imported_constructors() || !aes_vectors() || !lzma_formats() ||
        !sevenzip_bounds() || !inflate_formats() || !zip_bounds() ||
        !manifest_bounds()) {
        return 1;
    }
    puts("Resource format and malformed-input checks passed.");
    return 0;
}
