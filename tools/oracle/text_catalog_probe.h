#ifndef AB_ORACLE_TEXT_CATALOG_H
#define AB_ORACLE_TEXT_CATALOG_H

#include "text_file_jni.h"

typedef struct {
    const uint32_t *group;
    void *resources;
    const void *group_name, *missing_group;
    const void *(*get)(const void *, const void *);
    const void *(*resource_get)(const void *, const void *, const void *);
} TextCatalogLookup;

static void text_catalog_emit_lookup(const TextCatalogLookup *lookup,
                                     const void *identifier) {
    uint32_t before = lookup->group[7];
    const void *direct = lookup->get(lookup->group, identifier);
    uint32_t after_direct = lookup->group[7];
    const void *resource =
        lookup->resource_get(lookup->resources, lookup->group_name, identifier);
    const void *missing =
        lookup->resource_get(lookup->resources, lookup->missing_group, identifier);
    text_catalog_emit_string(direct);
    text_catalog_emit_string(resource);
    text_catalog_emit_string(missing);
    uint32_t metadata[] = {before, after_direct, lookup->group[7], direct == resource,
                           missing == identifier};
    print_words(metadata, 5);
}

static void text_catalog_emit_map(const TextCatalogLookup *lookup) {
    const uint32_t *group = lookup->group;
    uint32_t count = group[7];
    if (count > 8192) {
        fail("Native TEXT group exceeds map bound\n");
    }
    print_words(&count, 1);
    const uint32_t *stack[64];
    size_t depth = 0, emitted = 0;
    const uint32_t *node = (const uint32_t *)(uintptr_t)group[4];
    while (node || depth) {
        while (node) {
            if (depth == 64) {
                fail("Native TEXT map exceeds depth bound\n");
            }
            stack[depth++] = node;
            node = (const uint32_t *)(uintptr_t)node[2];
        }
        node = stack[--depth];
        if (++emitted > count) {
            fail("Native TEXT map contains excess nodes\n");
        }
        text_catalog_emit_string(node + 4);
        text_catalog_emit_string(node + 5);
        text_catalog_emit_lookup(lookup, node + 4);
        node = (const uint32_t *)(uintptr_t)node[3];
    }
    if (emitted != count) {
        fail("Native TEXT map count mismatch\n");
    }
    static const char *const queries[] = {"",
                                          "__AB520_MISSING_TEXT__",
                                          "__AB520_MISSING_TEXT__",
                                          "TEXTS_BASIC",
                                          "AB520_MISSING_\xc3\xa9",
                                          "A\0B",
                                          " ",
                                          ""};
    const size_t lengths[] = {0, 22, 22, 11, 16, 3, 1, 0};
    uint32_t query_count = 8;
    print_words(&query_count, 1);
    for (size_t index = 0; index < 8; ++index) {
        TextCatalogFixtureString fixture;
        text_catalog_fixture(&fixture, queries[index], lengths[index]);
        const char *identifier = fixture.bytes;
        text_catalog_emit_string(&identifier);
        text_catalog_emit_lookup(lookup, &identifier);
    }
}

static void probe_text_catalog(void) {
    text_catalog_initialize_jni();
    void (*construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN4game12TextGroupSetC1ERKSs");
    void (*load_locales)(void *) =
        (void (*)(void *))find_function("_ZN4game12TextGroupSet15loadLocaleCodesEv");
    void *(*load_group)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN4game12TextGroupSet13loadTextGroupERKSs");
    const uint32_t *(*get_group)(const void *, const void *) =
        (const uint32_t *(*)(const void *, const void *))find_function(
            "_ZNK4game12TextGroupSet12getTextGroupERKSs");
    void (*construct_group)(void *) =
        (void (*)(void *))find_function("_ZN4game9TextGroupC1Ev");
    TextCatalogLookup lookup = {
        .get = (const void *(*)(const void *, const void *))find_function(
            "_ZNK4game9TextGroup3getERKSs"),
        .resource_get =
            (const void *(*)(const void *, const void *, const void *))find_function(
                "_ZNK4game9Resources9getStringERKSsS2_")};
    _Alignas(8) uint32_t type_probe[8] = {0};
    construct_group(type_probe);
    semihost(4, "AB_ANDROID_TEXT_CATALOG_V2 4\n");
    for (size_t file = 0; file < 4; ++file) {
        TextCatalogFixtureString filename = {.capacity = 63, .references = -1};
        while (text_catalog_files[file][filename.length]) {
            if (filename.length >= 63) {
                fail("Native TEXT catalog name exceeds bounds\n");
            }
            filename.bytes[filename.length] = text_catalog_files[file][filename.length];
            ++filename.length;
        }
        const char *name = filename.bytes;
        text_catalog_emit_string(&name);
        _Alignas(8) uint32_t group_set[12] = {0};
        construct(group_set, &name);
        load_locales(group_set);
        /* Lookup-only Resources snapshot; the original parser owns each set/map.
         * getString reads this single-node group map and explicit locale only.
         * No constructor/bootstrap/destructor behavior is claimed for Resources. */
        TextCatalogFixtureString group_name, missing_name;
        text_catalog_fixture(&group_name, filename.bytes, filename.length - 4);
        text_catalog_fixture(&missing_name, "__AB520_GROUP_MISSING__", 23);
        const char *group_text = group_name.bytes, *missing_text = missing_name.bytes;
        _Alignas(8) uint32_t resources[0x468 / 4] = {0};
        uint32_t node[6] = {0};
        node[1] = (uint32_t)(uintptr_t)(resources + 0x450 / 4);
        node[4] = (uint32_t)(uintptr_t)group_text;
        node[5] = (uint32_t)(uintptr_t)group_set;
        resources[0x454 / 4] = (uint32_t)(uintptr_t)node;
        resources[0x458 / 4] = (uint32_t)(uintptr_t)node;
        resources[0x45c / 4] = (uint32_t)(uintptr_t)node;
        resources[0x460 / 4] = 1;
        lookup.resources = resources;
        lookup.group_name = &group_text;
        lookup.missing_group = &missing_text;
        if (group_set[3] < group_set[2] || (group_set[3] - group_set[2]) % 4) {
            fail("Invalid native TEXT locale vector\n");
        }
        uint32_t count = (group_set[3] - group_set[2]) / 4;
        if (!count || count > 128) {
            fail("Native TEXT locale count exceeds bounds\n");
        }
        print_words(&count, 1);
        for (uint32_t index = 0; index < count; ++index) {
            const void *locale = (const void *)(uintptr_t)(group_set[2] + index * 4);
            text_catalog_emit_string(locale);
            (void)load_group(group_set, locale);
            const uint32_t *group = get_group(group_set, locale);
            if (!group || group[0] != type_probe[0]) {
                fail("Native TEXT locale did not load\n");
            }
            resources[0x448 / 4] = *(const uint32_t *)locale;
            lookup.group = group;
            text_catalog_emit_map(&lookup);
        }
    }
    uint32_t metadata[] = {text_file_reads, text_vm_calls, text_vm_version};
    print_words(metadata, 3);
    uint32_t count = 0;
    for (size_t index = 0; index < 233; ++index) {
        count += text_jni_counts[index] != 0;
    }
    print_words(&count, 1);
    for (uint32_t index = 0; index < 233; ++index) {
        if (text_jni_counts[index]) {
            uint32_t call[] = {index, text_jni_counts[index]};
            print_words(call, 2);
        }
    }
    semihost(4, "AB text catalog reference complete\n");
}

#endif
