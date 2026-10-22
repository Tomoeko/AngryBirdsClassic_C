#include "ab/text.h"

#include "ab/dat.h"

#include <stdlib.h>
#include <string.h>

static bool allocate_spans(size_t count, size_t *budget, AbSpan **output,
                           AbError *error) {
    *output = NULL;
    if (count > *budget / sizeof(**output)) {
        return ab_fail(error, "TEXT span array exceeds allocation limit");
    }
    if (count) {
        *output = calloc(count, sizeof(**output));
        if (!*output) {
            return ab_fail(error, "Cannot allocate TEXT spans");
        }
    }
    *budget -= count * sizeof(**output);
    return true;
}

static bool read_strings(AbDatReader *reader, AbSpan *strings, size_t count,
                         AbError *error) {
    for (size_t index = 0; index < count; ++index) {
        if (!ab_dat_string(reader, strings + index, error)) {
            return false;
        }
    }
    return reader->offset == reader->bytes.size ||
           ab_fail(error, "Trailing TEXT string-list data");
}

static bool read_list(AbSpan payload, size_t *budget, AbSpan **output, size_t *count,
                      AbError *error) {
    AbDatReader reader = {payload, 0};
    int16_t length;
    if (!ab_dat_i16(&reader, &length, error) || length < 0 ||
        (size_t)length > (payload.size - reader.offset) / 2) {
        return ab_fail(error, "Invalid TEXT list count");
    }
    *count = (size_t)length;
    return allocate_spans(*count, budget, output, error) &&
           read_strings(&reader, *output, *count, error);
}

static bool read_catalog(AbSpan payload, size_t budget, AbTextCatalog *catalog,
                         AbError *error) {
    AbDatReader reader = {payload, 0};
    uint16_t version;
    if (!ab_dat_u16(&reader, &version, error) || version != 1) {
        return ab_fail(error, "Unsupported TEXT version");
    }
    bool has_locales = false;
    bool has_ids = false;
    size_t groups = 0;
    while (reader.offset < reader.bytes.size) {
        AbDatChunk chunk;
        if (!ab_dat_next(&reader, &chunk, error)) {
            return false;
        }
        if (memcmp(chunk.tag.data, "LDAT", 4) == 0) {
            if (has_locales || groups) {
                return ab_fail(error, "Duplicate or late TEXT locale list");
            }
            if (!read_list(chunk.payload, &budget, &catalog->locales,
                           &catalog->locale_count, error)) {
                return false;
            }
            has_locales = true;
        } else if (memcmp(chunk.tag.data, "LIDS", 4) == 0) {
            if (has_ids || groups) {
                return ab_fail(error, "Duplicate or late TEXT identifier list");
            }
            if (!read_list(chunk.payload, &budget, &catalog->ids, &catalog->id_count,
                           error)) {
                return false;
            }
            has_ids = true;
        } else if (memcmp(chunk.tag.data, "TXGP", 4) == 0) {
            if (!has_locales || !has_ids || !catalog->id_count ||
                groups >= catalog->locale_count) {
                return ab_fail(error, "TEXT translation group has no matching header");
            }
            if (!groups) {
                if (catalog->locale_count > SIZE_MAX / catalog->id_count ||
                    !allocate_spans(catalog->locale_count * catalog->id_count, &budget,
                                    &catalog->translations, error)) {
                    return ab_fail(error, "TEXT translation matrix exceeds bounds");
                }
            }
            AbDatReader strings = {chunk.payload, 0};
            if (!read_strings(&strings,
                              catalog->translations + groups * catalog->id_count,
                              catalog->id_count, error)) {
                return false;
            }
            ++groups;
        }
    }
    return (has_locales && has_ids && groups == catalog->locale_count) ||
           ab_fail(error, "Missing TEXT headers or translation groups");
}

bool ab_text_read(AbSpan input, size_t allocation_limit, AbTextCatalog *output,
                  AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing text catalog output");
    }
    *output = (AbTextCatalog){0};
    AbDatReader reader;
    if (!ab_dat_open(input, "KA3D", &reader, error)) {
        return false;
    }
    AbTextCatalog catalog = {0};
    bool found = false;
    while (reader.offset < reader.bytes.size) {
        AbDatChunk chunk;
        if (!ab_dat_next(&reader, &chunk, error)) {
            goto release_catalog;
        }
        if (memcmp(chunk.tag.data, "TEXT", 4) != 0) {
            continue;
        }
        if (found) {
            ab_fail(error, "Multiple TEXT chunks are unsupported");
            goto release_catalog;
        }
        if (!read_catalog(chunk.payload, allocation_limit, &catalog, error)) {
            goto release_catalog;
        }
        found = true;
    }
    if (!found) {
        ab_fail(error, "DAT file has no TEXT chunk");
        goto release_catalog;
    }
    *output = catalog;
    return true;

release_catalog:
    ab_text_free(&catalog);
    return false;
}

static size_t find_string(const AbSpan *strings, size_t count, AbSpan key) {
    for (size_t remaining = count; remaining; --remaining) {
        AbSpan candidate = strings[remaining - 1];
        if (candidate.size == key.size &&
            (!key.size || memcmp(candidate.data, key.data, key.size) == 0)) {
            return remaining - 1;
        }
    }
    return SIZE_MAX;
}

bool ab_text_find(const AbTextCatalog *catalog, AbSpan locale, AbSpan id,
                  AbSpan *output) {
    if (!output) {
        return false;
    }
    *output = (AbSpan){0};
    if (!catalog || (locale.size && !locale.data) || (id.size && !id.data)) {
        return false;
    }
    size_t locale_index = find_string(catalog->locales, catalog->locale_count, locale);
    size_t id_index = find_string(catalog->ids, catalog->id_count, id);
    if (locale_index == SIZE_MAX || id_index == SIZE_MAX) {
        return false;
    }
    *output = catalog->translations[locale_index * catalog->id_count + id_index];
    return true;
}

void ab_text_free(AbTextCatalog *catalog) {
    if (catalog) {
        free(catalog->translations);
        free(catalog->ids);
        free(catalog->locales);
        *catalog = (AbTextCatalog){0};
    }
}
