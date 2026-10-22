#ifndef AB_ORACLE_TEXT_LOCALE_H
#define AB_ORACLE_TEXT_LOCALE_H

#include "text_file_jni.h"

static uint32_t locale_java_class, locale_java_default_method, locale_java_method;
static uint32_t locale_java_default, locale_java_string;
static const char *locale_java_bytes;
static size_t locale_java_length;

static void *locale_find_class(void *environment, const char *name) {
    if (!text_catalog_equal(name, "java/util/Locale")) {
        return text_jni_find_class(environment, name);
    }
    text_jni_check(environment, 6);
    return &locale_java_class;
}

static void *locale_static_method(void *environment, void *klass, const char *name,
                                  const char *signature) {
    if (klass != &locale_java_class) {
        return text_jni_method_id(environment, klass, name, signature);
    }
    text_jni_check(environment, 113);
    if (!text_catalog_equal(name, "getDefault") ||
        !text_catalog_equal(signature, "()Ljava/util/Locale;")) {
        fail("Unexpected native locale static method\n");
    }
    return &locale_java_default_method;
}

static void *locale_method(void *environment, void *klass, const char *name,
                           const char *signature) {
    text_jni_check(environment, 33);
    if (klass != &locale_java_class || !text_catalog_equal(name, "toString") ||
        !text_catalog_equal(signature, "()Ljava/lang/String;")) {
        fail("Unexpected native locale instance method\n");
    }
    return &locale_java_method;
}

static void *locale_call_static(void *environment, void *klass, void *method,
                                const void *arguments) {
    if (klass != &locale_java_class) {
        return text_jni_call_static_v(environment, klass, method, arguments);
    }
    text_jni_check(environment, 115);
    if (method != &locale_java_default_method) {
        fail("Unexpected native locale static call\n");
    }
    return &locale_java_default;
}

static void *locale_call_object(void *environment, void *object, void *method,
                                const void *arguments) {
    text_jni_check(environment, 35);
    (void)arguments;
    if (object != &locale_java_default || method != &locale_java_method) {
        fail("Unexpected native locale instance call\n");
    }
    return &locale_java_string;
}

static int32_t locale_string_length(void *environment, void *object) {
    text_jni_check(environment, 164);
    if (object != &locale_java_string || locale_java_length > 63) {
        fail("Unexpected native locale string length\n");
    }
    return (int32_t)locale_java_length;
}

static int32_t locale_string_utf_length(void *environment, void *object) {
    text_jni_check(environment, 168);
    if (object != &locale_java_string || locale_java_length > 63) {
        fail("Unexpected native locale UTF length\n");
    }
    return (int32_t)locale_java_length;
}

static void locale_string_region(void *environment, void *object, int32_t start,
                                 int32_t length, char *output) {
    text_jni_check(environment, 221);
    if (object != &locale_java_string || start != 0 || length < 0 ||
        (size_t)length != locale_java_length || !output) {
        fail("Unexpected native locale UTF region\n");
    }
    for (size_t index = 0; index < locale_java_length; ++index) {
        output[index] = locale_java_bytes[index];
    }
}

static void probe_text_locale(void) {
    text_catalog_initialize_jni();
    text_jni_functions[6] = (uintptr_t)locale_find_class;
    text_jni_functions[33] = (uintptr_t)locale_method;
    text_jni_functions[35] = (uintptr_t)locale_call_object;
    text_jni_functions[113] = (uintptr_t)locale_static_method;
    text_jni_functions[115] = (uintptr_t)locale_call_static;
    text_jni_functions[164] = (uintptr_t)locale_string_length;
    text_jni_functions[168] = (uintptr_t)locale_string_utf_length;
    text_jni_functions[221] = (uintptr_t)locale_string_region;
    void (*construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN4game12TextGroupSetC1ERKSs");
    void (*load_locales)(void *) =
        (void (*)(void *))find_function("_ZN4game12TextGroupSet15loadLocaleCodesEv");
    void (*select_locale)(void *) =
        (void (*)(void *))find_function("_ZN7GameLua16getCurrentLocaleEv");
    TextCatalogFixtureString filename, empty, group_name;
    text_catalog_fixture(&filename, "TEXTS_BASIC.dat", 15);
    text_catalog_fixture(&empty, "", 0);
    text_catalog_fixture(&group_name, "TEXTS_BASIC", 11);
    const char *name = filename.bytes, *empty_text = empty.bytes;
    _Alignas(8) uint32_t group_set[12] = {0}, empty_set[12] = {0};
    construct(group_set, &name);
    construct(empty_set, &empty_text);
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    claim(group_set);
    load_locales(group_set);
    if (group_set[3] < group_set[2] || (group_set[3] - group_set[2]) % 4) {
        fail("Unexpected native locale source vector\n");
    }
    uint32_t available_count = (group_set[3] - group_set[2]) / 4;
    if (!available_count || available_count > 32) {
        fail("Native locale source count exceeds bound\n");
    }
    uint32_t available[32] = {0};
    for (uint32_t index = 0; index < available_count; ++index) {
        available[index] = ((const uint32_t *)(uintptr_t)group_set[2])[index];
    }
    _Alignas(8) uint32_t resources[0x468 / 4] = {0};
    uint32_t node[6] = {0};
    node[1] = (uint32_t)(uintptr_t)(resources + 0x450 / 4);
    node[4] = (uint32_t)(uintptr_t)group_name.bytes;
    node[5] = (uint32_t)(uintptr_t)group_set;
    resources[0x448 / 4] = empty_set[11];
    resources[0x454 / 4] = (uint32_t)(uintptr_t)node;
    resources[0x458 / 4] = (uint32_t)(uintptr_t)node;
    resources[0x45c / 4] = (uint32_t)(uintptr_t)node;
    resources[0x460 / 4] = 1;
    _Alignas(8) uint32_t game[0x54 / 4] = {0};
    game[0x50 / 4] = (uint32_t)(uintptr_t)resources;
    static const char *const specials[] = {
        "",
        "j",
        "ja",
        "ja_US",
        "ja_JP",
        "japanese",
        "JA_JP",
        "Ja_JP",
        "en_US",
        "en",
        "en_EN",
        "fr",
        "FR_FR",
        "unknown",
        "de_DE_extra",
        "zh_Hans_CN",
        "ja-XX",
        "ja_",
        "jazz",
        "jaXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        "en_EN ",
        " en_EN",
        "fr-FR",
        "JA",
        "en_EN_US",
        "ja_JP_extra"};
    semihost(4, "AB_ANDROID_TEXT_LOCALE_V1 4096\n");
    print_words(&available_count, 1);
    for (uint32_t index = 0; index < available_count; ++index) {
        text_catalog_emit_string(available + index);
    }
    uint32_t state = 0x52010ca1;
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t selected[32] = {0}, count = 0;
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        uint32_t mask = state;
        if (test % 8 == 0) {
            mask = 0;
        } else if (test % 8 == 1) {
            mask = UINT32_MAX;
        }
        for (uint32_t index = 0; index < available_count; ++index) {
            if (mask & (UINT32_C(1) << index)) {
                selected[count++] = available[index];
            }
        }
        group_set[2] = (uint32_t)(uintptr_t)selected;
        group_set[3] = group_set[2] + count * 4;
        group_set[4] = group_set[3];
        TextCatalogFixtureString preferred;
        if (test % 3 == 0) {
            locale_java_bytes =
                (const char *)(uintptr_t)available[test % available_count];
            locale_java_length = ((const uint32_t *)locale_java_bytes)[-3];
        } else {
            locale_java_bytes = specials[test % (sizeof(specials) / sizeof(*specials))];
            locale_java_length = 0;
            while (locale_java_bytes[locale_java_length]) {
                ++locale_java_length;
            }
        }
        text_catalog_fixture(&preferred, locale_java_bytes, locale_java_length);
        const char *preferred_text = preferred.bytes;
        print_words(&mask, 1);
        text_catalog_emit_string(&preferred_text);
        select_locale(game);
        text_catalog_emit_string(resources + 0x448 / 4);
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
    semihost(4, "AB text locale reference complete\n");
}

#endif
