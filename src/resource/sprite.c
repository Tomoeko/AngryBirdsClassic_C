#include "ab/sprite.h"

#include "ab/dat.h"

#include <stdlib.h>
#include <string.h>

static bool read_regions(AbSpan payload, size_t allocation_limit, AbSpriteSheet *sheet,
                         AbError *error) {
    AbDatReader reader = {payload, 0};
    uint16_t version;
    int16_t count;
    if (!ab_dat_u16(&reader, &version, error) || version != 1) {
        return ab_fail(error, "Unsupported SPRT version");
    }
    if (!ab_dat_string(&reader, &sheet->texture, error) ||
        !ab_dat_i16(&reader, &count, error) || count < 0) {
        return ab_fail(error, "Invalid SPRT texture or region count");
    }
    size_t region_count = (size_t)count;
    if (region_count > allocation_limit / sizeof(*sheet->sprites) ||
        region_count > (payload.size - reader.offset) / 14) {
        return ab_fail(error, "SPRT regions exceed allocation or input bounds");
    }
    if (region_count) {
        sheet->sprites = calloc(region_count, sizeof(*sheet->sprites));
        if (!sheet->sprites) {
            return ab_fail(error, "Cannot allocate SPRT regions");
        }
    }
    sheet->count = region_count;
    for (size_t index = 0; index < region_count; ++index) {
        AbSpriteRegion *region = sheet->sprites + index;
        if (!ab_dat_string(&reader, &region->name, error) ||
            !ab_dat_i16(&reader, &region->x, error) ||
            !ab_dat_i16(&reader, &region->y, error) ||
            !ab_dat_i16(&reader, &region->width, error) ||
            !ab_dat_i16(&reader, &region->height, error) ||
            !ab_dat_i16(&reader, &region->pivot_x, error) ||
            !ab_dat_i16(&reader, &region->pivot_y, error)) {
            return false;
        }
    }
    return reader.offset == payload.size || ab_fail(error, "Trailing SPRT data");
}

bool ab_sprite_sheet_read(AbSpan input, size_t allocation_limit, AbSpriteSheet *output,
                          AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing sprite sheet output");
    }
    *output = (AbSpriteSheet){0};
    AbDatReader reader;
    if (!ab_dat_open(input, "KA3D", &reader, error)) {
        return false;
    }
    AbSpriteSheet sheet = {0};
    bool found = false;
    while (reader.offset < reader.bytes.size) {
        AbDatChunk chunk;
        if (!ab_dat_next(&reader, &chunk, error)) {
            goto release_sheet;
        }
        if (memcmp(chunk.tag.data, "SPRT", 4) != 0) {
            continue;
        }
        if (found) {
            ab_fail(error, "Multiple SPRT chunks are unsupported");
            goto release_sheet;
        }
        if (!read_regions(chunk.payload, allocation_limit, &sheet, error)) {
            goto release_sheet;
        }
        found = true;
    }
    if (!found) {
        ab_fail(error, "DAT file has no SPRT chunk");
        goto release_sheet;
    }
    *output = sheet;
    return true;

release_sheet:
    ab_sprite_sheet_free(&sheet);
    return false;
}

const AbSpriteRegion *ab_sprite_find(const AbSpriteSheet *sheet, AbSpan name) {
    if (!sheet || (name.size && !name.data)) {
        return NULL;
    }
    /* Native insertion replaces the named region; preserve the last definition. */
    for (size_t remaining = sheet->count; remaining; --remaining) {
        const AbSpriteRegion *region = sheet->sprites + remaining - 1;
        if (region->name.size == name.size &&
            (!name.size || memcmp(region->name.data, name.data, name.size) == 0)) {
            return region;
        }
    }
    return NULL;
}

void ab_sprite_sheet_free(AbSpriteSheet *sheet) {
    if (sheet) {
        free(sheet->sprites);
        *sheet = (AbSpriteSheet){0};
    }
}
