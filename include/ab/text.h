#ifndef AB_TEXT_H
#define AB_TEXT_H

#include "ab/base.h"

typedef struct {
    AbSpan *locales;
    AbSpan *ids;
    /* Locale-major matrix: translations[locale_index * id_count + id_index]. */
    AbSpan *translations;
    size_t locale_count;
    size_t id_count;
} AbTextCatalog;

/* Owns span arrays; all string bytes borrow input. Output starts empty. */
bool ab_text_read(AbSpan input, size_t allocation_limit, AbTextCatalog *output,
                  AbError *error);
/* Exact lookup only. A present empty translation returns true with size zero. */
bool ab_text_find(const AbTextCatalog *catalog, AbSpan locale, AbSpan id,
                  AbSpan *output);
void ab_text_free(AbTextCatalog *catalog);

#endif
