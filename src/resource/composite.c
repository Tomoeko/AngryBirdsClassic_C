#include "ab/composite.h"

#include "ab/dat.h"

#include <stdlib.h>
#include <string.h>

static bool read_count(AbDatReader *reader, size_t minimum_bytes, size_t *output,
                       AbError *error) {
    int16_t count;
    if (!ab_dat_i16(reader, &count, error) || count < 0 ||
        (size_t)count > (reader->bytes.size - reader->offset) / minimum_bytes) {
        return ab_fail(error, "Invalid COMP record count");
    }
    *output = (size_t)count;
    return true;
}

static void *allocate_records(size_t count, size_t element_size, size_t *budget,
                              AbError *error) {
    if (count > *budget / element_size) {
        ab_fail(error, "COMP records exceed allocation limit");
        return NULL;
    }
    if (!count) {
        return NULL;
    }
    void *memory = calloc(count, element_size);
    if (!memory) {
        ab_fail(error, "Cannot allocate COMP records");
        return NULL;
    }
    *budget -= count * element_size;
    return memory;
}

static bool read_part(AbDatReader *reader, bool extended, AbCompositePart *part,
                      AbError *error) {
    if (!ab_dat_string(reader, &part->sprite, error)) {
        return false;
    }
    part->tag = (AbSpan){0};
    if (extended && !ab_dat_string(reader, &part->tag, error)) {
        return false;
    }
    if (!ab_dat_i16(reader, &part->x, error) || !ab_dat_i16(reader, &part->y, error)) {
        return false;
    }
    part->scale_x_bits = 0x3f800000;
    part->scale_y_bits = 0x3f800000;
    if (extended) {
        AbSpan flags;
        if (!ab_dat_u32(reader, &part->scale_x_bits, error) ||
            !ab_dat_u32(reader, &part->scale_y_bits, error) ||
            !ab_dat_u32(reader, &part->rotation_bits, error) ||
            !ab_dat_bytes(reader, 2, &flags, error)) {
            return false;
        }
        part->flip_x = flags.data[0] != 0;
        part->flip_y = flags.data[1] != 0;
    }
    return true;
}

static bool read_composite(AbDatReader *reader, bool extended, uint16_t version,
                           size_t *budget, AbComposite *composite, AbError *error) {
    if (!ab_dat_string(reader, &composite->name, error) ||
        !read_count(reader, extended ? 22 : 6, &composite->part_count, error)) {
        return false;
    }
    composite->parts = allocate_records(composite->part_count,
                                        sizeof(*composite->parts), budget, error);
    if (composite->part_count && !composite->parts) {
        return false;
    }
    for (size_t index = 0; index < composite->part_count; ++index) {
        if (!read_part(reader, extended, composite->parts + index, error)) {
            return false;
        }
    }
    if (!extended && version == 2) {
        if (!read_count(reader, 6, &composite->point_count, error)) {
            return false;
        }
        composite->points = allocate_records(composite->point_count,
                                             sizeof(*composite->points), budget, error);
        if (composite->point_count && !composite->points) {
            return false;
        }
        for (size_t index = 0; index < composite->point_count; ++index) {
            AbCompositePoint *point = composite->points + index;
            if (!ab_dat_string(reader, &point->name, error) ||
                !ab_dat_i16(reader, &point->x, error) ||
                !ab_dat_i16(reader, &point->y, error)) {
                return false;
            }
        }
    }
    return true;
}

static bool read_set(AbSpan payload, size_t budget, AbCompositeSet *set,
                     AbError *error) {
    AbDatReader reader = {payload, 0};
    if (!ab_dat_u16(&reader, &set->version, error) ||
        (set->version != 1 && (set->extended || set->version != 2))) {
        return ab_fail(error, "Unsupported COMP version");
    }
    size_t minimum_bytes = !set->extended && set->version == 2 ? 6 : 4;
    if (!read_count(&reader, minimum_bytes, &set->count, error)) {
        return false;
    }
    set->composites =
        allocate_records(set->count, sizeof(*set->composites), &budget, error);
    if (set->count && !set->composites) {
        return false;
    }
    for (size_t index = 0; index < set->count; ++index) {
        if (!read_composite(&reader, set->extended, set->version, &budget,
                            set->composites + index, error)) {
            return false;
        }
    }
    return reader.offset == payload.size || ab_fail(error, "Trailing COMP data");
}

bool ab_composite_read(AbSpan input, size_t allocation_limit, AbCompositeSet *output,
                       AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing composite output");
    }
    *output = (AbCompositeSet){0};
    if (!input.data || input.size < 8 ||
        (memcmp(input.data, "KA3D", 4) != 0 && memcmp(input.data, "RVIO", 4) != 0)) {
        return ab_fail(error, "Unsupported COMP container");
    }
    AbCompositeSet set = {0};
    set.extended = memcmp(input.data, "RVIO", 4) == 0;
    AbDatReader reader;
    if (!ab_dat_open(input, set.extended ? "RVIO" : "KA3D", &reader, error)) {
        return false;
    }
    bool found = false;
    while (reader.offset < reader.bytes.size) {
        AbDatChunk chunk;
        if (!ab_dat_next(&reader, &chunk, error)) {
            goto release_set;
        }
        if (memcmp(chunk.tag.data, "COMP", 4) != 0) {
            continue;
        }
        if (found) {
            ab_fail(error, "Multiple COMP chunks are unsupported");
            goto release_set;
        }
        if (!read_set(chunk.payload, allocation_limit, &set, error)) {
            goto release_set;
        }
        found = true;
    }
    if (!found) {
        ab_fail(error, "DAT file has no COMP chunk");
        goto release_set;
    }
    *output = set;
    return true;

release_set:
    ab_composite_free(&set);
    return false;
}

void ab_composite_free(AbCompositeSet *set) {
    if (set) {
        if (set->composites) {
            for (size_t index = 0; index < set->count; ++index) {
                free(set->composites[index].parts);
                free(set->composites[index].points);
            }
        }
        free(set->composites);
        *set = (AbCompositeSet){0};
    }
}
