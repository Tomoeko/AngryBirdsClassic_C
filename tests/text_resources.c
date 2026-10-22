#include "ab/text_resources.h"
#include "ab/ui_localization.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "TEXT resources failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool equals(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool resource_checks(void) {
    AbSpan locales[] = {span("en_EN"), span("ja_JP")};
    AbSpan ids[] = {span("id"), span("blank"), span("id")};
    AbSpan translations[] = {span("old"), span(""),  span("English"),
                             span("old"), span(" "), span("Japanese")};
    AbTextCatalog catalog = {locales, ids, translations, 2, 3};
    AbTextResourceGroup groups[] = {{span("BASIC"), NULL}, {span("BASIC"), &catalog}};
    AbTextResources resources = {groups, 2, locales[1]};
    AbSpan output;
    CHECK(ab_text_resources_get(&resources, span("BASIC"), ids[0], &output, NULL) &&
          equals(output, translations[5]));
    CHECK(ab_text_resources_get(&resources, span("BASIC"), ids[1], &output, NULL) &&
          equals(output, translations[4]));
    resources.locale = locales[0];
    CHECK(ab_text_resources_get(&resources, span("BASIC"), ids[1], &output, NULL) &&
          !output.size && output.data);
    const uint8_t binary[] = {'A', 0, 'B'};
    AbSpan requested = {binary, sizeof(binary)};
    CHECK(ab_text_resources_get(&resources, span("BASIC"), requested, &output, NULL) &&
          output.data == requested.data && output.size == requested.size);
    resources.locale = span("unknown");
    CHECK(ab_text_resources_get(&resources, span("missing"), ids[0], &output, NULL) &&
          output.data == ids[0].data && output.size == ids[0].size);
    output = span("sentinel");
    CHECK(!ab_text_resources_get(&resources, span("BASIC"), ids[0], &output, NULL) &&
          equals(output, span("sentinel")));
    resources.group_count = 1;
    CHECK(!ab_text_resources_get(&resources, span("BASIC"), ids[0], &output, NULL) &&
          equals(output, span("sentinel")));
    CHECK(!ab_text_resources_get(NULL, span("BASIC"), ids[0], &output, NULL));
    CHECK(!ab_text_resources_get(&resources, span("BASIC"), ids[0], NULL, NULL));
    CHECK(!ab_text_resources_get(&resources, (AbSpan){NULL, 1}, ids[0], &output, NULL));
    resources.group_count = 0;
    CHECK(ab_text_resources_get(&resources, span("BASIC"), ids[0], &output, NULL) &&
          equals(output, ids[0]));
    resources.group_count = 1;
    resources.groups = NULL;
    CHECK(!ab_text_resources_get(&resources, span("BASIC"), ids[0], &output, NULL));
    return true;
}

static bool locale_checks(void) {
    AbSpan available[] = {span("fr_FR"), span("ja_JP")}, output;
    CHECK(ab_text_locale_select(available, 2, span("fr_FR"), &output, NULL) &&
          output.data == available[0].data);
    CHECK(ab_text_locale_select(available, 2, span("ja_US"), &output, NULL) &&
          output.data == available[1].data);
    CHECK(ab_text_locale_select(available, 2, span("jazz"), &output, NULL) &&
          equals(output, span("ja_JP")));
    CHECK(ab_text_locale_select(available, 2, span("JA_JP"), &output, NULL) &&
          equals(output, span("en_EN")));
    CHECK(ab_text_locale_select(available, 1, span("ja_JP"), &output, NULL) &&
          equals(output, span("en_EN")));
    CHECK(ab_text_locale_select(NULL, 0, span(""), &output, NULL) &&
          equals(output, span("en_EN")));
    available[0] = span("");
    CHECK(ab_text_locale_select(available, 1, span(""), &output, NULL) && !output.size);
    output = span("sentinel");
    CHECK(!ab_text_locale_select(NULL, 1, span("en_EN"), &output, NULL) &&
          equals(output, span("sentinel")));
    available[0] = (AbSpan){NULL, 1};
    CHECK(!ab_text_locale_select(available, 1, span("en_EN"), &output, NULL) &&
          equals(output, span("sentinel")));
    CHECK(!ab_text_locale_select(NULL, 0, (AbSpan){NULL, 1}, &output, NULL));
    CHECK(!ab_text_locale_select(NULL, 0, span("en_EN"), NULL, NULL));
    CHECK(!ab_text_locale_select(available, 32768, span("en_EN"), &output, NULL));
    return true;
}

static bool localization_checks(void) {
    AbSpan locales[] = {span("en_EN")};
    AbSpan ids[] = {span("id"), span("blank"), span("")};
    AbSpan base_values[] = {span("id"), span("blank"), span("base-empty")};
    AbSpan extra_values[] = {span("translated"), span(""), span("extra-empty")};
    AbTextCatalog base = {locales, ids, base_values, 1, 3};
    AbTextCatalog extra = {locales, ids, extra_values, 1, 3};
    AbTextResourceGroup groups[] = {{span("TEXTS_BASIC"), &base},
                                    {span("TEXTS_APPRATER"), &extra}};
    AbTextResources resources = {groups, 2, locales[0]};
    AbSpan profile[] = {groups[0].name, groups[1].name};
    AbUiLocalizationSource source = {profile, 2, &resources, ab_text_resources_lookup};
    AbUiLocalizationCache cache = {0};
    AbSpan output;
    CHECK(ab_ui_localization_string(&source, ids[0], &cache, &output, NULL) &&
          equals(output, extra_values[0]) && cache.present &&
          equals(cache.group, groups[1].name));
    CHECK(ab_ui_localization_string(&source, ids[1], &cache, &output, NULL) &&
          !output.size && equals(cache.group, groups[1].name));
    CHECK(ab_ui_localization_string(&source, ids[2], &cache, &output, NULL) &&
          equals(output, base_values[2]) && equals(cache.group, groups[1].name));
    CHECK(ab_ui_localization_string(&source, span("missing"), &cache, &output, NULL) &&
          equals(output, span("missing")) && equals(cache.group, groups[1].name));
    resources.locale = span("unavailable");
    output = span("sentinel");
    CHECK(!ab_ui_localization_string(&source, ids[0], &cache, &output, NULL) &&
          equals(output, span("sentinel")) && equals(cache.group, groups[1].name));
    return true;
}

int main(void) {
    if (!resource_checks() || !locale_checks() || !localization_checks()) {
        return 1;
    }
    puts("TEXT resource empty/missing/duplicate lookup, locale errors, binary IDs, "
         "borrowed output and UI localization composition checks pass.");
    return 0;
}
