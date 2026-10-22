#ifndef AB_TEXT_RESOURCES_H
#define AB_TEXT_RESOURCES_H

#include "ab/text.h"

typedef struct {
    AbSpan name;
    const AbTextCatalog *catalog;
} AbTextResourceGroup;

typedef struct {
    const AbTextResourceGroup *groups;
    size_t group_count;
    AbSpan locale;
} AbTextResources;

/* Android GameLua locale choice after the platform has supplied its one default
 * Locale.toString value and TEXTS_BASIC locales are available. A lowercase "ja"
 * prefix becomes "ja_JP" before exact matching. Any other unavailable value
 * selects "en_EN", even when that fallback is absent from available locales.
 * Stable valid text spans, count <=32767. Output borrows available or a static
 * fallback; failure preserves output. Java string conversion, resource loading
 * and preferred-language lists from other platforms are separate boundaries. */
bool ab_text_locale_select(const AbSpan *available, size_t count,
                           AbSpan android_default, AbSpan *output, AbError *error);

/* Read-only resolved Resources.getString boundary. Groups, catalogs, locale and
 * identifier remain borrowed. The last matching group wins. An unknown group
 * or identifier returns the identifier bytes; an existing empty translation
 * remains empty. A known group's missing locale fails rather than choosing a
 * different locale. Output is preserved on failure. No I/O or allocation.
 * Original missing-ID map insertion and locale loading remain outside this
 * value-only boundary. Prepared catalog arrays must come from ab_text_read. */
bool ab_text_resources_get(const AbTextResources *resources, AbSpan group,
                           AbSpan identifier, AbSpan *output, AbError *error);

/* Compatible with AbUiLocalizationSource.get_string; context borrows resources. */
bool ab_text_resources_lookup(void *context, AbSpan group, AbSpan identifier,
                              AbSpan *output, AbError *error);

#endif
