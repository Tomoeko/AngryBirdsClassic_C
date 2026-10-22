#include "ab/ui_localization.h"

#include <string.h>

static bool valid_text(AbSpan value) {
    return !value.size || value.data;
}

static bool same_text(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool lookup(const AbUiLocalizationSource *source, AbSpan group,
                   AbSpan identifier, AbSpan *output, AbError *error) {
    AbSpan result;
    if (!source->get_string(source->context, group, identifier, &result, error)) {
        return false;
    }
    if (!valid_text(result)) {
        return ab_fail(error, "Invalid UI localized text result");
    }
    *output = result;
    return true;
}

bool ab_ui_localization_string(const AbUiLocalizationSource *source, AbSpan identifier,
                               AbUiLocalizationCache *cache, AbSpan *output,
                               AbError *error) {
    if (!source || !source->get_string || !output || !valid_text(identifier) ||
        source->group_count > UINT32_C(0x1000000) ||
        (source->group_count && !source->groups) ||
        (cache && cache->present && !valid_text(cache->group))) {
        return ab_fail(error, "Invalid UI localization source or identifier");
    }
    for (size_t index = 0; index < source->group_count; ++index) {
        if (!valid_text(source->groups[index])) {
            return ab_fail(error, "Invalid UI localization profile group");
        }
    }
    if (identifier.size) {
        AbSpan result;
        if (cache && cache->present) {
            if (!lookup(source, cache->group, identifier, &result, error)) {
                return false;
            }
            if (!same_text(result, identifier)) {
                *output = result;
                return true;
            }
        }
        for (size_t index = 0; index < source->group_count; ++index) {
            AbSpan group = source->groups[index];
            if (cache && cache->present && same_text(group, cache->group)) {
                continue;
            }
            if (!lookup(source, group, identifier, &result, error)) {
                return false;
            }
            if (!same_text(result, identifier)) {
                if (cache) {
                    cache->group = group;
                    cache->present = true;
                }
                *output = result;
                return true;
            }
        }
    }
    const AbSpan basic = {(const uint8_t *)"TEXTS_BASIC", 11};
    AbSpan result;
    if (!lookup(source, basic, identifier, &result, error)) {
        return false;
    }
    *output = result;
    return true;
}
