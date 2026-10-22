#include "ab/font_wrap.h"

#include <limits.h>
#include <math.h>

static size_t candidate_end(const uint32_t *codes, size_t length, size_t start) {
    for (size_t index = start; index < length; ++index) {
        uint32_t code = codes[index];
        if (code == '-' || code == '\n' || code == ' ' || code == 0x200b) {
            return index + (code == '-');
        }
    }
    return length;
}

static size_t skip_spaces(const uint32_t *codes, size_t length, size_t start) {
    while (start < length && (codes[start] == ' ' || codes[start] == 0x200b)) {
        ++start;
    }
    return start;
}

static bool measure_width(const AbBitmapFont *font, const uint32_t *codes,
                          size_t length, size_t start, size_t end, int32_t *width,
                          AbError *error) {
    AbFontMeasure measure;
    if (!ab_font_measure_codes(font, codes, length, start, end - start, &measure)) {
        return ab_fail(error, "Invalid wrapped-text font measurement");
    }
    *width = measure.width;
    return true;
}

static bool split_word(const AbBitmapFont *font, const uint32_t *codes, size_t length,
                       size_t start, float limit, size_t *end, AbError *error) {
    for (size_t index = start; index < length; ++index) {
        int32_t width = 0;
        if (!measure_width(font, codes, length, start, index + 1, &width, error)) {
            return false;
        }
        if ((float)width >= limit) {
            *end = index;
            return true;
        }
    }
    return ab_fail(error, "Wrapped-text split cannot reach the width boundary");
}

static bool select_line(const AbBitmapFont *font, const uint32_t *codes, size_t length,
                        size_t start, float limit, AbFontWrappedLine *line,
                        size_t *next, AbError *error) {
    size_t cursor = start, accepted_end = start, end;
    bool accepted = false;
    for (;;) {
        end = candidate_end(codes, length, cursor);
        int32_t width = 0;
        if (!measure_width(font, codes, length, start, end, &width, error)) {
            return false;
        }
        if ((float)width >= limit) {
            if (accepted) {
                end = accepted_end;
            } else if (!split_word(font, codes, length, start, limit, &end, error)) {
                return false;
            } else {
                cursor = end;
            }
            break;
        }
        if (end == length || codes[end] == '\n') {
            cursor = end;
            break;
        }
        cursor = skip_spaces(codes, length, end);
        if (cursor == length) {
            break;
        }
        accepted_end = end;
        accepted = true;
    }
    if (cursor < length && codes[cursor] == '\n') {
        ++cursor;
    }
    if (cursor <= start) {
        return ab_fail(error,
                       "Wrapped-text width/font cannot advance past a character");
    }
    int32_t width = 0;
    if (!measure_width(font, codes, length, start, end, &width, error)) {
        return false;
    }
    *line = (AbFontWrappedLine){start, end - start, width};
    *next = cursor;
    return true;
}

bool ab_font_wrap_codes(const AbBitmapFont *font, const uint32_t *codes, size_t length,
                        float width_limit, AbFontWrap *output, AbError *error) {
    if (!output || (output->capacity && !output->lines) || isnan(width_limit)) {
        return ab_fail(error, "Invalid wrapped-text output or width");
    }
    AbFontWrap result = *output;
    result.count = 0;
    result.widest_line = 0;
    if (width_limit <= 0) {
        if (!result.capacity) {
            return ab_fail(error, "Wrapped-text output has no empty-line capacity");
        }
        result.lines[result.count++] = (AbFontWrappedLine){0};
        *output = result;
        return true;
    }
    if (length > INT32_MAX || (length && (!font || !codes))) {
        return ab_fail(error, "Invalid wrapped-text input storage or length");
    }
    size_t start = 0;
    while (start < length) {
        if (result.count == result.capacity) {
            return ab_fail(error, "Wrapped-text line capacity exceeded");
        }
        AbFontWrappedLine line = {0};
        size_t next = 0;
        if (!select_line(font, codes, length, start, width_limit, &line, &next,
                         error)) {
            return false;
        }
        result.lines[result.count++] = line;
        if (line.width > result.widest_line) {
            result.widest_line = line.width;
        }
        start = next;
    }
    *output = result;
    return true;
}
