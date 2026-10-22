#include "ab/json.h"

bool ab_json_bytes(FILE *file, AbSpan value) {
    if (!file || (!value.data && value.size)) {
        return false;
    }
    fputc('"', file);
    for (size_t index = 0; index < value.size; ++index) {
        unsigned byte = value.data[index];
        if (byte == '"' || byte == '\\') {
            fputc('\\', file);
            fputc((int)byte, file);
        } else if (byte < 32 || byte >= 127) {
            fprintf(file, "\\u%04x", byte);
        } else {
            fputc((int)byte, file);
        }
    }
    fputc('"', file);
    return !ferror(file);
}
