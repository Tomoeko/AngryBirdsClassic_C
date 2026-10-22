#ifndef AB_ORACLE_TEXT_FILE_JNI_H
#define AB_ORACLE_TEXT_FILE_JNI_H

#include <stdbool.h>

/* Original native TEXT parsing runs only in this reference process. The narrow
 * JNI adapter supplies FileReader.readFile as a bounded read-only byte buffer;
 * no original Java/file-decoding behavior is claimed. All parsed locale tables,
 * UTF strings, maps and get() results come from the pinned native library. */
static uintptr_t text_jni_functions[233], text_vm_functions[8];
static uintptr_t *text_jni_table = text_jni_functions;
static uintptr_t *text_vm_table = text_vm_functions;
static uint32_t text_jni_counts[233], text_vm_calls, text_vm_version;
static uint32_t text_jni_class, text_jni_method, text_jni_buffer;
static uint8_t text_file_bytes[16 * 1024 * 1024];
static size_t text_file_size;
static uint32_t text_file_reads;
static size_t text_emitted_bytes;
static const char *const text_catalog_files[] = {
    "TEXTS_APPRATER.dat", "TEXTS_BASIC.dat", "TEXTS_SUGOTOKU_CUSTOMISATION.dat",
    "TEXTS_UPDATE.dat"};

typedef struct {
    uint32_t length, capacity;
    int32_t references;
    char bytes[64];
} TextCatalogFixtureString;

static void text_catalog_fixture(TextCatalogFixtureString *output, const char *bytes,
                                 size_t length) {
    if (length >= sizeof(output->bytes)) {
        fail("Native TEXT fixture string exceeds bounds\n");
    }
    *output = (TextCatalogFixtureString){
        .length = (uint32_t)length, .capacity = 63, .references = -1};
    for (size_t index = 0; index < length; ++index) {
        output->bytes[index] = bytes[index];
    }
}

static bool text_catalog_equal(const char *first, const char *second) {
    if (!first || !second) {
        return false;
    }
    for (size_t index = 0; index < 256; ++index) {
        if (first[index] != second[index]) {
            return false;
        }
        if (!first[index]) {
            return true;
        }
    }
    return false;
}

static _Noreturn void text_jni_unsupported(void) {
    fail("Native TEXT parser reached an unaudited JNI operation\n");
}

static void text_jni_check(void *environment, size_t operation) {
    if (environment != &text_jni_table || operation >= 233) {
        fail("Unexpected native TEXT JNI environment\n");
    }
    ++text_jni_counts[operation];
}

static int text_vm_get_env(void *vm, void **output, int version) {
    if (vm != &text_vm_table || !output || (version != 0x10004 && version != 0x10006)) {
        fail("Unexpected native TEXT JavaVM request\n");
    }
    ++text_vm_calls;
    text_vm_version = (uint32_t)version;
    *output = &text_jni_table;
    return 0;
}

static void *text_jni_find_class(void *environment, const char *name) {
    text_jni_check(environment, 6);
    if (!text_catalog_equal(name, "com/rovio/fusion/FileReader")) {
        fail("Unexpected native TEXT Java class\n");
    }
    return &text_jni_class;
}

static void *text_jni_global(void *environment, void *object) {
    text_jni_check(environment, 21);
    return object;
}

static void text_jni_delete_global(void *environment, void *object) {
    text_jni_check(environment, 22);
    (void)object;
}

static void text_jni_delete_local(void *environment, void *object) {
    text_jni_check(environment, 23);
    (void)object;
}

static void *text_jni_method_id(void *environment, void *klass, const char *name,
                                const char *signature) {
    text_jni_check(environment, 113);
    if (klass != &text_jni_class || !text_catalog_equal(name, "readFile") ||
        !text_catalog_equal(signature, "(Ljava/lang/String;)Ljava/nio/ByteBuffer;")) {
        fail("Unexpected native TEXT Java method\n");
    }
    return &text_jni_method;
}

static void *text_jni_new_string(void *environment, const char *value) {
    text_jni_check(environment, 167);
    if (!value) {
        fail("Missing native TEXT filename\n");
    }
    return (void *)value;
}

static void text_catalog_read_file(const char *requested) {
    const char *basename = requested;
    size_t length = 0;
    while (requested[length]) {
        if (length >= 255) {
            fail("Native TEXT filename exceeds bounds\n");
        }
        if (requested[length] == '/') {
            basename = requested + length + 1;
        }
        ++length;
    }
    size_t file = 4;
    for (size_t index = 0; index < 4; ++index) {
        if (text_catalog_equal(basename, text_catalog_files[index])) {
            file = index;
            break;
        }
    }
    if (file == 4 || text_file_reads >= 1024) {
        fail("Native TEXT requested an unsupported file\n");
    }
    char path[128] = "/data/local/tmp/ab520/text/raw/";
    size_t position = 0;
    while (path[position]) {
        ++position;
    }
    for (size_t index = 0;; ++index) {
        if (position + index >= sizeof(path)) {
            fail("Native TEXT private path exceeds bounds\n");
        }
        path[position + index] = text_catalog_files[file][index];
        if (!text_catalog_files[file][index]) {
            break;
        }
    }
    int descriptor = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (descriptor < 0) {
        fail("Cannot open native TEXT input\n");
    }
    text_file_size = 0;
    for (;;) {
        if (text_file_size == sizeof(text_file_bytes)) {
            fail("Native TEXT input exceeds byte-buffer capacity\n");
        }
        size_t available = sizeof(text_file_bytes) - text_file_size;
        if (available > 65536) {
            available = 65536;
        }
        int count =
            system_call(3, descriptor, text_file_bytes + text_file_size, available);
        if (count < 0 || (size_t)count > available) {
            fail("Cannot read native TEXT input\n");
        }
        if (!count) {
            break;
        }
        text_file_size += (size_t)count;
    }
    if (system_call(6, descriptor, NULL, 0) || text_file_size < 8 ||
        text_file_bytes[0] != 'K' || text_file_bytes[1] != 'A' ||
        text_file_bytes[2] != '3' || text_file_bytes[3] != 'D') {
        fail("Expected an original KA3D text catalog\n");
    }
    ++text_file_reads;
}

static void *text_jni_call_static_v(void *environment, void *klass, void *method,
                                    const void *arguments) {
    text_jni_check(environment, 115);
    if (klass != &text_jni_class || method != &text_jni_method || !arguments) {
        fail("Unexpected native TEXT file-reader call\n");
    }
    const char *requested = *(const char *const *)arguments;
    if (!requested) {
        fail("Missing native TEXT file-reader argument\n");
    }
    text_catalog_read_file(requested);
    return &text_jni_buffer;
}

static uint8_t text_jni_exception_check(void *environment) {
    text_jni_check(environment, 228);
    return 0;
}

static void *text_jni_buffer_address(void *environment, void *buffer) {
    text_jni_check(environment, 230);
    if (buffer != &text_jni_buffer) {
        fail("Unexpected native TEXT direct buffer\n");
    }
    return text_file_bytes;
}

static int64_t text_jni_buffer_capacity(void *environment, void *buffer) {
    text_jni_check(environment, 231);
    if (buffer != &text_jni_buffer) {
        fail("Unexpected native TEXT direct-buffer capacity request\n");
    }
    return (int64_t)text_file_size;
}

static void text_catalog_initialize_jni(void) {
    for (size_t index = 0; index < 233; ++index) {
        text_jni_functions[index] = (uintptr_t)text_jni_unsupported;
    }
    for (size_t index = 0; index < 8; ++index) {
        text_vm_functions[index] = (uintptr_t)text_jni_unsupported;
    }
    text_vm_functions[6] = (uintptr_t)text_vm_get_env;
    text_jni_functions[6] = (uintptr_t)text_jni_find_class;
    text_jni_functions[21] = (uintptr_t)text_jni_global;
    text_jni_functions[22] = (uintptr_t)text_jni_delete_global;
    text_jni_functions[23] = (uintptr_t)text_jni_delete_local;
    text_jni_functions[113] = (uintptr_t)text_jni_method_id;
    text_jni_functions[115] = (uintptr_t)text_jni_call_static_v;
    text_jni_functions[167] = (uintptr_t)text_jni_new_string;
    text_jni_functions[228] = (uintptr_t)text_jni_exception_check;
    text_jni_functions[230] = (uintptr_t)text_jni_buffer_address;
    text_jni_functions[231] = (uintptr_t)text_jni_buffer_capacity;
    int (*on_load)(void *, void *) =
        (int (*)(void *, void *))find_function("JNI_OnLoad");
    if (on_load(&text_vm_table, NULL) != 0x10006) {
        fail("Unexpected original JNI initialization version\n");
    }
}

static void text_catalog_emit_string(const void *reference) {
    const uint8_t *value = *(const uint8_t *const *)reference;
    if (!value) {
        fail("Missing native TEXT string\n");
    }
    uint32_t length = ((const uint32_t *)value)[-3];
    if (length > 1024 * 1024 || length > 64 * 1024 * 1024 - text_emitted_bytes) {
        fail("Native TEXT string exceeds output bound\n");
    }
    text_emitted_bytes += length;
    print_words(&length, 1);
    static const char hex[] = "0123456789abcdef";
    for (size_t offset = 0; offset < length;) {
        size_t count = length - offset;
        if (count > 64) {
            count = 64;
        }
        char buffer[129];
        for (size_t index = 0; index < count; ++index) {
            buffer[index * 2] = hex[value[offset + index] >> 4];
            buffer[index * 2 + 1] = hex[value[offset + index] & 15];
        }
        buffer[count * 2] = 0;
        semihost(4, buffer);
        offset += count;
    }
    semihost(4, "\n");
}

#endif
