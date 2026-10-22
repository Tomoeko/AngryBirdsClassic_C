#include "ab/axml.h"

#include <string.h>

#define NO_STRING UINT32_MAX
enum { MAX_DEPTH = 64 };

typedef struct {
    AbSpan bytes;
    uint32_t count;
    uint32_t flags;
    size_t offsets_start;
    size_t strings_start;
    size_t strings_end;
} StringPool;

typedef struct {
    StringPool pool;
    AbManifest result;
    uint32_t stack[MAX_DEPTH];
    size_t depth;
    size_t activity_depth;
    size_t filter_depth;
    char activity[192];
    bool main_action;
    bool launcher_category;
    bool saw_root;
    bool finished_root;
    unsigned required_fields;
} ManifestReader;

static bool length8(AbSpan bytes, size_t *offset, uint32_t *length, AbError *error) {
    if (!ab_range(bytes.size, *offset, 1)) {
        return ab_fail(error, "Truncated XML UTF-8 string length");
    }
    *length = bytes.data[(*offset)++];
    if (*length & 128) {
        if (!ab_range(bytes.size, *offset, 1)) {
            return ab_fail(error, "Truncated XML UTF-8 string length");
        }
        *length = ((*length & 127) << 8) | bytes.data[(*offset)++];
    }
    return true;
}

static bool length16(AbSpan bytes, size_t *offset, uint32_t *length, AbError *error) {
    if (!ab_range(bytes.size, *offset, 2)) {
        return ab_fail(error, "Truncated XML UTF-16 string length");
    }
    *length = ab_u16le(bytes.data + *offset);
    *offset += 2;
    if (*length & 32768) {
        if (!ab_range(bytes.size, *offset, 2)) {
            return ab_fail(error, "Truncated XML UTF-16 string length");
        }
        *length = ((*length & 32767) << 16) | ab_u16le(bytes.data + *offset);
        *offset += 2;
    }
    return true;
}

static bool copy_utf8(AbSpan bytes, uint32_t units, char *output, size_t capacity,
                      AbError *error) {
    if (bytes.size >= capacity) {
        return ab_fail(error, "XML string exceeds supported output length");
    }
    size_t index = 0;
    size_t measured_units = 0;
    while (index < bytes.size) {
        uint32_t value = bytes.data[index++];
        unsigned extra = 0;
        uint32_t minimum = 0;
        if (value >= 0xc2 && value <= 0xdf) {
            extra = 1;
            value &= 31;
            minimum = 128;
        } else if (value >= 0xe0 && value <= 0xef) {
            extra = 2;
            value &= 15;
            minimum = 2048;
        } else if (value >= 0xf0 && value <= 0xf4) {
            extra = 3;
            value &= 7;
            minimum = 65536;
        } else if (value >= 128 || value == 0) {
            return ab_fail(error, "Invalid XML UTF-8 leading byte");
        }
        if (!ab_range(bytes.size, index, extra)) {
            return ab_fail(error, "Truncated XML UTF-8 sequence");
        }
        for (unsigned part = 0; part < extra; ++part) {
            uint8_t byte = bytes.data[index++];
            if ((byte & 0xc0) != 0x80) {
                return ab_fail(error, "Invalid XML UTF-8 continuation");
            }
            value = (value << 6) | (byte & 63);
        }
        if (value < minimum || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff)) {
            return ab_fail(error, "Invalid XML Unicode scalar");
        }
        measured_units += value > 65535 ? 2 : 1;
    }
    if (measured_units != units) {
        return ab_fail(error, "XML UTF-8 and UTF-16 lengths disagree");
    }
    memcpy(output, bytes.data, bytes.size);
    output[bytes.size] = '\0';
    return true;
}

static bool copy_utf16(AbSpan bytes, char *output, size_t capacity, AbError *error) {
    size_t written = 0;
    for (size_t offset = 0; offset < bytes.size; offset += 2) {
        uint32_t value = ab_u16le(bytes.data + offset);
        if (value >= 0xd800 && value <= 0xdbff) {
            if (!ab_range(bytes.size, offset + 2, 2)) {
                return ab_fail(error, "Truncated XML surrogate pair");
            }
            uint32_t low = ab_u16le(bytes.data + offset + 2);
            if (low < 0xdc00 || low > 0xdfff) {
                return ab_fail(error, "Invalid XML surrogate pair");
            }
            value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
            offset += 2;
        } else if ((value >= 0xdc00 && value <= 0xdfff) || value == 0) {
            return ab_fail(error, "Invalid XML UTF-16 scalar");
        }
        size_t count = value < 128 ? 1 : (value < 2048 ? 2 : (value < 65536 ? 3 : 4));
        if (count >= capacity - written) {
            return ab_fail(error, "XML string exceeds supported output length");
        }
        if (count == 1) {
            output[written++] = (char)value;
        } else {
            uint8_t prefix = count == 2 ? 0xc0 : (count == 3 ? 0xe0 : 0xf0);
            output[written++] = (char)(prefix | (value >> ((count - 1) * 6)));
            for (size_t part = count - 1; part > 0; --part) {
                output[written++] = (char)(0x80 | ((value >> ((part - 1) * 6)) & 63));
            }
        }
    }
    output[written] = '\0';
    return true;
}

static bool pool_string(const StringPool *pool, uint32_t index, char *output,
                        size_t capacity, AbError *error) {
    if (index >= pool->count || !capacity) {
        return ab_fail(error, "XML string index is out of range");
    }
    size_t relative = ab_u32le(pool->bytes.data + pool->offsets_start + index * 4);
    if (relative >= pool->strings_end - pool->strings_start) {
        return ab_fail(error, "XML string offset is out of range");
    }
    AbSpan bytes = {pool->bytes.data, pool->strings_end};
    size_t offset = pool->strings_start + relative;
    uint32_t units = 0;
    if (pool->flags & 256) {
        uint32_t length = 0;
        if (!length8(bytes, &offset, &units, error) ||
            !length8(bytes, &offset, &length, error)) {
            return false;
        }
        if (!ab_range(bytes.size, offset, (size_t)length + 1) ||
            bytes.data[offset + length] != 0) {
            return ab_fail(error, "XML UTF-8 string is truncated or unterminated");
        }
        return copy_utf8((AbSpan){bytes.data + offset, length}, units, output, capacity,
                         error);
    }
    if (!length16(bytes, &offset, &units, error)) {
        return false;
    }
    if (units > (bytes.size - offset) / 2 ||
        !ab_range(bytes.size, offset, (size_t)units * 2 + 2) ||
        ab_u16le(bytes.data + offset + (size_t)units * 2) != 0) {
        return ab_fail(error, "XML UTF-16 string is truncated or unterminated");
    }
    return copy_utf16((AbSpan){bytes.data + offset, (size_t)units * 2}, output,
                      capacity, error);
}

static bool read_pool(AbSpan chunk, size_t header, StringPool *pool, AbError *error) {
    if (header < 28) {
        return ab_fail(error, "XML string pool header is truncated");
    }
    uint32_t count = ab_u32le(chunk.data + 8);
    uint32_t styles = ab_u32le(chunk.data + 12);
    size_t start = ab_u32le(chunk.data + 20);
    size_t style_start = ab_u32le(chunk.data + 24);
    if (start < header || start > chunk.size || count > (start - header) / 4 ||
        styles > (start - header) / 4 - count ||
        (style_start && (style_start < start || style_start > chunk.size))) {
        return ab_fail(error, "XML string pool tables exceed chunk bounds");
    }
    *pool = (StringPool){chunk,  count, ab_u32le(chunk.data + 16),
                         header, start, style_start ? style_start : chunk.size};
    if (pool->flags & ~UINT32_C(0x101)) {
        return ab_fail(error, "Unsupported XML string-pool flags");
    }
    char scratch[4096];
    for (uint32_t index = 0; index < count; ++index) {
        if (!pool_string(pool, index, scratch, sizeof(scratch), error)) {
            return false;
        }
    }
    return true;
}

static bool copy_field(char *destination, size_t capacity, const char *value,
                       AbError *error) {
    size_t length = strlen(value);
    if (length >= capacity) {
        return ab_fail(error, "Manifest field exceeds supported length");
    }
    memcpy(destination, value, length + 1);
    return true;
}

static bool apply_attribute(ManifestReader *reader, const char *element,
                            const char *name, uint32_t namespace_index, uint8_t type,
                            uint32_t number, const char *text, AbError *error) {
    bool android = false;
    if (namespace_index != NO_STRING) {
        char uri[256];
        if (!pool_string(&reader->pool, namespace_index, uri, sizeof(uri), error)) {
            return false;
        }
        android = strcmp(uri, "http://schemas.android.com/apk/res/android") == 0;
    }
    AbManifest *result = &reader->result;
    bool numeric = type == 0x10 || type == 0x11;
    if (reader->depth == 0 && strcmp(element, "manifest") == 0) {
        if (namespace_index == NO_STRING && strcmp(name, "package") == 0 && type == 3) {
            reader->required_fields |= 1;
            return copy_field(result->package, sizeof(result->package), text, error);
        }
        if (android && strcmp(name, "versionName") == 0 && type == 3) {
            reader->required_fields |= 2;
            return copy_field(result->version_name, sizeof(result->version_name), text,
                              error);
        }
        if (android && strcmp(name, "versionCode") == 0 && numeric) {
            reader->required_fields |= 4;
            result->version_code = number;
        }
    }
    if (reader->depth == 1 && strcmp(element, "uses-sdk") == 0 && android) {
        if (strcmp(name, "minSdkVersion") == 0 && numeric) {
            reader->required_fields |= 8;
            result->min_sdk = number;
        }
        if (strcmp(name, "targetSdkVersion") == 0 && numeric) {
            reader->required_fields |= 16;
            result->target_sdk = number;
        }
    }
    if (reader->depth == 1 && strcmp(element, "uses-feature") == 0 && android &&
        strcmp(name, "glEsVersion") == 0 && numeric) {
        result->gles_version = number;
    }
    if (reader->activity_depth == reader->depth + 1 && android &&
        strcmp(name, "name") == 0 && type == 3) {
        return copy_field(reader->activity, sizeof(reader->activity), text, error);
    }
    if (reader->filter_depth && reader->depth == reader->filter_depth && android &&
        strcmp(name, "name") == 0 && type == 3) {
        if (strcmp(element, "action") == 0 &&
            strcmp(text, "android.intent.action.MAIN") == 0) {
            reader->main_action = true;
        }
        if (strcmp(element, "category") == 0 &&
            strcmp(text, "android.intent.category.LAUNCHER") == 0) {
            reader->launcher_category = true;
        }
    }
    return true;
}

static bool start_element(ManifestReader *reader, AbSpan chunk, size_t header,
                          AbError *error) {
    if (header != 16 || !ab_range(chunk.size, header, 20) ||
        reader->depth >= MAX_DEPTH || reader->finished_root) {
        return ab_fail(error, "Invalid XML element header or nesting depth");
    }
    const uint8_t *extension = chunk.data + header;
    uint32_t name_index = ab_u32le(extension + 4);
    char element[128];
    if (!pool_string(&reader->pool, name_index, element, sizeof(element), error)) {
        return false;
    }
    if (!reader->depth) {
        if (reader->saw_root || strcmp(element, "manifest") != 0) {
            return ab_fail(error, "XML root must be a single manifest element");
        }
        reader->saw_root = true;
    }
    if (strcmp(element, "activity") == 0 || strcmp(element, "activity-alias") == 0) {
        ++reader->result.activity_count;
        reader->activity_depth = reader->depth + 1;
        reader->activity[0] = '\0';
    } else if (strcmp(element, "service") == 0) {
        ++reader->result.service_count;
    } else if (strcmp(element, "receiver") == 0) {
        ++reader->result.receiver_count;
    } else if (strcmp(element, "intent-filter") == 0 && reader->activity_depth &&
               reader->depth == reader->activity_depth) {
        reader->filter_depth = reader->depth + 1;
        reader->main_action = false;
        reader->launcher_category = false;
    }
    size_t attribute_start = ab_u16le(extension + 8);
    size_t attribute_size = ab_u16le(extension + 10);
    size_t count = ab_u16le(extension + 12);
    if (attribute_start < 20 || attribute_size < 20 ||
        !ab_range(chunk.size, header, attribute_start) ||
        count > (chunk.size - header - attribute_start) / attribute_size) {
        return ab_fail(error, "XML attributes exceed element bounds");
    }
    for (size_t index = 0; index < count; ++index) {
        const uint8_t *attribute = extension + attribute_start + index * attribute_size;
        uint32_t namespace_index = ab_u32le(attribute);
        uint32_t raw = ab_u32le(attribute + 8);
        uint8_t type = attribute[15];
        uint32_t value = ab_u32le(attribute + 16);
        char name[128];
        char text[4096] = {0};
        if (ab_u16le(attribute + 12) != 8 || attribute[14] != 0 ||
            !pool_string(&reader->pool, ab_u32le(attribute + 4), name, sizeof(name),
                         error)) {
            return ab_fail(error, "Invalid XML attribute name or typed value");
        }
        if (raw != NO_STRING &&
            !pool_string(&reader->pool, raw, text, sizeof(text), error)) {
            return false;
        }
        if (type == 3 &&
            !pool_string(&reader->pool, value, text, sizeof(text), error)) {
            return false;
        }
        if (!apply_attribute(reader, element, name, namespace_index, type, value, text,
                             error)) {
            return false;
        }
    }
    reader->stack[reader->depth++] = name_index;
    return true;
}

static bool end_element(ManifestReader *reader, AbSpan chunk, size_t header,
                        AbError *error) {
    if (header != 16 || !ab_range(chunk.size, header, 8) || !reader->depth ||
        ab_u32le(chunk.data + header + 4) != reader->stack[reader->depth - 1]) {
        return ab_fail(error, "XML end element does not match open element");
    }
    if (reader->depth == reader->filter_depth) {
        if (reader->main_action && reader->launcher_category) {
            if (reader->result.launcher[0] || !reader->activity[0]) {
                return ab_fail(error, "Ambiguous or unnamed manifest launcher");
            }
            memcpy(reader->result.launcher, reader->activity, sizeof(reader->activity));
        }
        reader->filter_depth = 0;
    }
    if (reader->depth == reader->activity_depth) {
        reader->activity_depth = 0;
    }
    --reader->depth;
    if (!reader->depth) {
        reader->finished_root = true;
    }
    return true;
}

bool ab_manifest_read(AbSpan input, AbManifest *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing manifest output");
    }
    *output = (AbManifest){0};
    if (!input.data || input.size < 8 || ab_u16le(input.data) != 3 ||
        ab_u16le(input.data + 2) != 8 || ab_u32le(input.data + 4) != input.size) {
        return ab_fail(error, "Invalid Android binary XML header");
    }
    ManifestReader reader = {0};
    size_t offset = 8;
    while (offset < input.size) {
        if (!ab_range(input.size, offset, 8)) {
            return ab_fail(error, "Truncated XML chunk header");
        }
        const uint8_t *bytes = input.data + offset;
        uint16_t type = ab_u16le(bytes);
        size_t header = ab_u16le(bytes + 2);
        size_t size = ab_u32le(bytes + 4);
        if (header < 8 || size < header || !ab_range(input.size, offset, size)) {
            return ab_fail(error, "Invalid XML chunk bounds");
        }
        AbSpan chunk = {bytes, size};
        if (type == 1) {
            if (reader.pool.bytes.data || reader.saw_root ||
                !read_pool(chunk, header, &reader.pool, error)) {
                return ab_fail(error, "Invalid or duplicate XML string pool");
            }
        } else if (!reader.pool.bytes.data) {
            return ab_fail(error, "XML node precedes string pool");
        } else if (type == 0x102) {
            if (!start_element(&reader, chunk, header, error)) {
                return false;
            }
        } else if (type == 0x103) {
            if (!end_element(&reader, chunk, header, error)) {
                return false;
            }
        } else if (type == 0x100 || type == 0x101) {
            if (header != 16 || !ab_range(size, header, 8)) {
                return ab_fail(error, "Truncated XML namespace record");
            }
            uint32_t prefix = ab_u32le(bytes + header);
            uint32_t uri = ab_u32le(bytes + header + 4);
            if ((prefix != NO_STRING && prefix >= reader.pool.count) ||
                uri >= reader.pool.count) {
                return ab_fail(error, "Invalid XML namespace string index");
            }
        } else if (type == 0x180) {
            if (header != 8 || (size - header) % 4 || reader.saw_root) {
                return ab_fail(error, "Invalid XML resource map");
            }
        } else {
            return ab_fail(error, "Unsupported Android XML chunk type 0x%04x", type);
        }
        offset += size;
    }
    if (reader.depth || !reader.finished_root || reader.required_fields != 31 ||
        !reader.result.launcher[0]) {
        return ab_fail(error, "Incomplete manifest or required fields are unsupported");
    }
    *output = reader.result;
    return true;
}
