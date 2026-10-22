#include "ab/text_resources.h"

#include <string.h>

static bool valid_text(AbSpan text) {
    return !text.size || text.data;
}

static bool same_text(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

bool ab_text_locale_select(const AbSpan *available, size_t count,
                           AbSpan android_default, AbSpan *output, AbError *error) {
    if (!output || count > 32767 || (count && !available) ||
        !valid_text(android_default)) {
        return ab_fail(error, "Invalid Android TEXT locale selection");
    }
    for (size_t index = 0; index < count; ++index) {
        if (!valid_text(available[index])) {
            return ab_fail(error, "Invalid available TEXT locale");
        }
    }
    const AbSpan japanese = {(const uint8_t *)"ja_JP", 5};
    if (android_default.size >= 2 && android_default.data[0] == 'j' &&
        android_default.data[1] == 'a') {
        android_default = japanese;
    }
    for (size_t index = 0; index < count; ++index) {
        if (same_text(available[index], android_default)) {
            *output = available[index];
            return true;
        }
    }
    *output = (AbSpan){(const uint8_t *)"en_EN", 5};
    return true;
}

bool ab_text_resources_get(const AbTextResources *resources, AbSpan group,
                           AbSpan identifier, AbSpan *output, AbError *error) {
    if (!resources || !output || !valid_text(group) || !valid_text(identifier) ||
        !valid_text(resources->locale) ||
        (resources->group_count && !resources->groups)) {
        return ab_fail(error, "Invalid TEXT resources lookup");
    }
    const AbTextCatalog *catalog = NULL;
    bool found = false;
    for (size_t remaining = resources->group_count; remaining; --remaining) {
        const AbTextResourceGroup *candidate = resources->groups + remaining - 1;
        if (!valid_text(candidate->name)) {
            return ab_fail(error, "Invalid TEXT resource group name");
        }
        if (same_text(candidate->name, group)) {
            catalog = candidate->catalog;
            found = true;
            break;
        }
    }
    if (!found) {
        *output = identifier;
        return true;
    }
    if (!catalog) {
        return ab_fail(error, "Missing TEXT resource catalog");
    }
    bool locale_present = false;
    for (size_t index = 0; index < catalog->locale_count; ++index) {
        locale_present |= same_text(catalog->locales[index], resources->locale);
    }
    if (!locale_present) {
        return ab_fail(error, "TEXT resource locale is unavailable");
    }
    AbSpan result;
    *output = ab_text_find(catalog, resources->locale, identifier, &result)
                  ? result
                  : identifier;
    return true;
}

bool ab_text_resources_lookup(void *context, AbSpan group, AbSpan identifier,
                              AbSpan *output, AbError *error) {
    return ab_text_resources_get(context, group, identifier, output, error);
}
