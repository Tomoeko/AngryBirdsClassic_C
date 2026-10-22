/* ARMv7 reference corpus; neither mode starts the Android application. The
 * isolated mode maps target segments, applies internal relocations, and guards
 * unsupported imports/accesses. Its math imports accept captured results or
 * explicit zero angles only in audited experiments. Android mode uses the real
 * platform loader, library constructors, and imports. Host runners verify inputs;
 * none of this reference execution is part of the first-party game runtime.
 */
#include <stddef.h>
#include <stdint.h>

/* Freestanding compiler helper; no C runtime is linked into the probe. */
void __aeabi_memclr8(void *destination, size_t size) {
    volatile uint8_t *bytes = destination;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

void __aeabi_memclr4(void *destination, size_t size) {
    __aeabi_memclr8(destination, size);
}

void __aeabi_memcpy8(void *destination, const void *source, size_t size) {
    volatile uint8_t *output = destination;
    const volatile uint8_t *input = source;
    for (size_t index = 0; index < size; ++index) {
        output[index] = input[index];
    }
}

#if defined(AB_ORACLE_ANDROID)
#include "android_oracle.h"
#else

enum {
    INPUT_ADDRESS = 0x42000000,
    MATH_ADDRESS = 0x43000000,
    LIBRARY_ADDRESS = 0x44000000,
    LIBRARY_CAPACITY = 24 * 1024 * 1024,
    INPUT_SIZE = 12170760,
};

static uint32_t semihost(uint32_t operation, const void *parameters) {
    register uint32_t result __asm__("r0") = operation;
    register const void *argument __asm__("r1") = parameters;
    __asm__ volatile("svc #0x123456" : "+r"(result) : "r"(argument) : "memory", "cc");
    return result;
}

static _Noreturn void finish(uint32_t status) {
    uint32_t parameters[] = {0x20026, status};
    semihost(0x20, parameters);
    for (;;) {
    }
}

static _Noreturn void fail(const char *message) {
    semihost(4, message);
    finish(1);
}

_Noreturn void oracle_abort(void) {
    fail("Target probe reached an unsupported ARM exception\n");
}

static uint16_t read16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static uint32_t read32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static const uint8_t *file_bytes(uint32_t offset, uint32_t size) {
    if (offset > INPUT_SIZE || size > INPUT_SIZE - offset) {
        fail("ELF file range exceeds pinned library\n");
    }
    return (const uint8_t *)(uintptr_t)INPUT_ADDRESS + offset;
}

static void load_segments(void) {
    const uint8_t *header = file_bytes(0, 52);
    if (read32(header) != 0x464c457f || header[4] != 1 || header[5] != 1 ||
        read16(header + 16) != 3 || read16(header + 18) != 40 ||
        read16(header + 42) != 32) {
        fail("Expected target ELF32 ARM shared library\n");
    }
    uint32_t offset = read32(header + 28);
    uint32_t count = read16(header + 44);
    const uint8_t *programs = file_bytes(offset, count * 32);
    for (uint32_t index = 0; index < count; ++index) {
        const uint8_t *program = programs + index * 32;
        if (read32(program) != 1) {
            continue;
        }
        uint32_t start = read32(program + 8);
        uint32_t size = read32(program + 16);
        uint32_t memory_size = read32(program + 20);
        if (size > memory_size || start > LIBRARY_CAPACITY ||
            memory_size > LIBRARY_CAPACITY - start) {
            fail("ELF segment exceeds probe memory\n");
        }
        const uint8_t *source = file_bytes(read32(program + 4), size);
        uint8_t *destination = (uint8_t *)(uintptr_t)LIBRARY_ADDRESS + start;
        for (uint32_t byte = 0; byte < memory_size; ++byte) {
            destination[byte] = byte < size ? source[byte] : 0;
        }
    }
}

static _Noreturn void unresolved_import(void) {
    fail("Audited target path reached an unsupported native import\n");
}

static uint32_t reference_math_enabled;

static int has_reference_math(void) {
    const uint8_t *table = (const uint8_t *)(uintptr_t)MATH_ADDRESS;
    uint32_t magic = read32(table);
    if (magic && (magic != UINT32_C(0x314d4241) || read32(table + 4) != 4096)) {
        fail("Unsupported reference math data\n");
    }
    return magic != 0;
}

static float reference_math_value(float angle, size_t column) {
    union {
        float number;
        uint32_t bits;
    } value = {.number = angle};
    if (!reference_math_enabled) {
        fail("Reference math imports are outside the audited experiment\n");
    }
    if (has_reference_math()) {
        const uint8_t *table = (const uint8_t *)(uintptr_t)MATH_ADDRESS + 8;
        for (size_t index = 0; index < 4096; ++index) {
            if (read32(table + index * 12) == value.bits) {
                value.bits = read32(table + index * 12 + column * 4);
                return value.number;
            }
        }
        fail("Target requested an uncaptured reference angle\n");
    }
    if (value.bits & UINT32_C(0x7fffffff)) {
        fail("Reference math requires a captured angle or explicit zero\n");
    }
    return column == 1 ? angle : 1.0f;
}

static float reference_sine(float angle) {
    return reference_math_value(angle, 1);
}

static float reference_cosine(float angle) {
    return reference_math_value(angle, 2);
}

static int symbol_equal(const uint8_t *bytes, uint32_t available, const char *name);

static void relocate_image(void) {
    const uint8_t *header = file_bytes(0, 52);
    if (read16(header + 46) != 40) {
        fail("Unexpected ELF relocation section layout\n");
    }
    uint32_t count = read16(header + 48);
    const uint8_t *sections = file_bytes(read32(header + 32), count * 40);
    for (uint32_t index = 0; index < count; ++index) {
        const uint8_t *section = sections + index * 40;
        if (read32(section + 4) != 9) {
            continue;
        }
        uint32_t size = read32(section + 20);
        uint32_t symbols_index = read32(section + 24);
        if (read32(section + 36) != 8 || size % 8 || symbols_index >= count) {
            fail("Unexpected target ELF relocation table\n");
        }
        const uint8_t *relocations = file_bytes(read32(section + 16), size);
        const uint8_t *symbols_section = sections + symbols_index * 40;
        uint32_t symbols_size = read32(symbols_section + 20);
        if (read32(symbols_section + 36) != 16 || symbols_size % 16) {
            fail("Unexpected target ELF relocation symbols\n");
        }
        const uint8_t *symbols = file_bytes(read32(symbols_section + 16), symbols_size);
        uint32_t strings_index = read32(symbols_section + 24);
        if (strings_index >= count) {
            fail("Invalid relocation symbol string table\n");
        }
        const uint8_t *strings_section = sections + strings_index * 40;
        uint32_t strings_size = read32(strings_section + 20);
        const uint8_t *strings = file_bytes(read32(strings_section + 16), strings_size);
        for (uint32_t entry = 0; entry < size / 8; ++entry) {
            const uint8_t *relocation = relocations + entry * 8;
            uint32_t address = read32(relocation);
            uint32_t info = read32(relocation + 4);
            uint32_t kind = info & 255;
            uint32_t symbol_index = info >> 8;
            if ((address & 3) || address > LIBRARY_CAPACITY - 4 ||
                symbol_index >= symbols_size / 16) {
                fail("Target ELF relocation exceeds probe image\n");
            }
            uint32_t *destination = (uint32_t *)(uintptr_t)(LIBRARY_ADDRESS + address);
            if (kind == 23) {
                if (*destination >= LIBRARY_CAPACITY) {
                    fail("Target ELF relative pointer exceeds probe image\n");
                }
                *destination += LIBRARY_ADDRESS;
            } else if (kind == 2 || kind == 21 || kind == 22) {
                const uint8_t *symbol = symbols + symbol_index * 16;
                if (read16(symbol + 14)) {
                    fail("Unexpected defined external relocation in pinned image\n");
                }
                /* Unsupported imports cannot silently supply reference values. */
                *destination = kind == 22 || (symbol[12] & 15) == 2
                                   ? (uint32_t)(uintptr_t)unresolved_import
                                   : UINT32_C(0xdead0000);
                uint32_t name = read32(symbol);
                if (kind == 22 && name < strings_size) {
                    if (symbol_equal(strings + name, strings_size - name, "sinf")) {
                        *destination = (uint32_t)(uintptr_t)reference_sine;
                    } else if (symbol_equal(strings + name, strings_size - name,
                                            "cosf")) {
                        *destination = (uint32_t)(uintptr_t)reference_cosine;
                    }
                }
            } else if (kind != 0) {
                fail("Unsupported target ELF relocation type\n");
            }
        }
    }
}

static int symbol_equal(const uint8_t *name, size_t limit, const char *wanted) {
    for (size_t index = 0; index < limit; ++index) {
        if (name[index] != (uint8_t)wanted[index]) {
            return 0;
        }
        if (!name[index]) {
            return 1;
        }
    }
    return 0;
}

static uintptr_t find_symbol(const char *name, unsigned kind) {
    const uint8_t *header = file_bytes(0, 52);
    if (read16(header + 46) != 40) {
        fail("Unexpected ELF section layout\n");
    }
    uint32_t section_count = read16(header + 48);
    const uint8_t *sections = file_bytes(read32(header + 32), section_count * 40);
    for (uint32_t index = 0; index < section_count; ++index) {
        const uint8_t *section = sections + index * 40;
        if (read32(section + 4) != 11) {
            continue;
        }
        uint32_t size = read32(section + 20);
        uint32_t strings_index = read32(section + 24);
        if (read32(section + 36) != 16 || size % 16 || strings_index >= section_count) {
            fail("Unexpected ELF dynamic symbols\n");
        }
        const uint8_t *symbols = file_bytes(read32(section + 16), size);
        const uint8_t *strings_section = sections + strings_index * 40;
        uint32_t strings_size = read32(strings_section + 20);
        const uint8_t *strings = file_bytes(read32(strings_section + 16), strings_size);
        for (uint32_t symbol = 0; symbol < size / 16; ++symbol) {
            const uint8_t *entry = symbols + symbol * 16;
            uint32_t string_offset = read32(entry);
            if (string_offset >= strings_size || !read16(entry + 14) ||
                (entry[12] & 15) != kind ||
                !symbol_equal(strings + string_offset, strings_size - string_offset,
                              name)) {
                continue;
            }
            uint32_t address = read32(entry + 4);
            if (address >= LIBRARY_CAPACITY) {
                fail("ELF symbol exceeds probe memory\n");
            }
            return (uintptr_t)LIBRARY_ADDRESS + address;
        }
    }
    fail("Required target symbol is missing\n");
}

static uintptr_t find_function(const char *name) {
    return find_symbol(name, 2);
}

static void initialize_target(void) {
    load_segments();
    relocate_image();
}

static void oracle_reference_rotation(size_t index, unsigned negative_zero,
                                      uint32_t *angle, uint32_t *sine,
                                      uint32_t *cosine) {
    *angle = negative_zero ? UINT32_C(0x80000000) : 0;
    *sine = *angle;
    *cosine = UINT32_C(0x3f800000);
    if (has_reference_math()) {
        const uint8_t *record =
            (const uint8_t *)(uintptr_t)MATH_ADDRESS + 8 + index * 12;
        *angle = read32(record);
        *sine = read32(record + 4);
        *cosine = read32(record + 8);
    }
}

static void oracle_math_enable(unsigned enabled) {
    reference_math_enabled = enabled;
}

#endif

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char output[256 * 9 + 1];
    if (count > 256) {
        fail("Probe record exceeds output buffer\n");
    }
    for (size_t word = 0; word < count; ++word) {
        for (unsigned index = 0; index < 8; ++index) {
            output[word * 9 + index] = digits[(words[word] >> ((7 - index) * 4)) & 15];
        }
        output[word * 9 + 8] = word + 1 == count ? '\n' : ' ';
    }
    output[count * 9] = 0;
    semihost(4, output);
}

static uint32_t random_word(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static uint32_t random_float(uint32_t *state, unsigned first, unsigned count,
                             int allow_negative) {
    uint32_t bits = random_word(state);
    uint32_t exponent = first + random_word(state) % count;
    return (bits & (allow_negative ? 0x807fffffu : 0x007fffffu)) | exponent << 23;
}

static uint32_t add_float(uint32_t first, uint32_t second) {
    union {
        float number;
        uint32_t bits;
    } left = {.bits = first}, right = {.bits = second};
    left.number = left.number + right.number;
    return left.bits;
}

typedef union {
    float number;
    uint32_t bits;
} FloatBits;

typedef void (*ShapeMass)(const void *, void *, float);
typedef void (*ShapeBounds)(const void *, void *, const void *, int);
typedef uint32_t (*ShapeContains)(const void *, const void *, const void *);
typedef void (*WorldQuery)(void *, const void *, const void *, float, const void *,
                           float);

static WorldQuery world_function(void) {
    return (WorldQuery)find_function(
        "_ZN15b2WorldManifold10InitializeEPK10b2ManifoldRK11b2TransformfS5_f");
}

static void record_world(WorldQuery query, uint32_t output[8],
                         const uint32_t manifold[16], const uint32_t first_transform[4],
                         uint32_t first_radius, const uint32_t second_transform[4],
                         uint32_t second_radius, size_t index) {
    for (uint32_t field = 0; field < 8; ++field) {
        output[field] = 0x3d000000u + (field << 12) + ((uint32_t)index & 255);
    }
    FloatBits first = {.bits = first_radius}, second = {.bits = second_radius};
    query(output, manifold, first_transform, first.number, second_transform,
          second.number);
}

static void initial_manifold(uint32_t words[16], size_t index) {
    for (uint32_t field = 0; field < 16; ++field) {
        words[field] = 0x3e800000u + (field << 12) + ((uint32_t)index & 255);
    }
    words[14] = 2;
    words[15] = 2;
}

static void transform_position(const uint32_t transform[4], const FloatBits point[2],
                               FloatBits output[2]) {
    FloatBits sine = {.bits = transform[2]}, cosine = {.bits = transform[3]};
    output[0].number =
        -(sine.number * point[1].number - cosine.number * point[0].number);
    output[1].number = cosine.number * point[1].number + sine.number * point[0].number;
    output[0].bits = add_float(output[0].bits, transform[0]);
    output[1].bits = add_float(output[1].bits, transform[1]);
}

static void probe_circle_contacts(void) {
    typedef void (*Collide)(void *, const void *, const void *, const void *,
                            const void *);
    Collide collide = (Collide)find_function(
        "_Z16b2CollideCirclesP10b2ManifoldPK13b2CircleShapeRK11b2TransformS3_S6_");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    WorldQuery initialize_world = world_function();
    uint32_t state = 0x520c0111;
    semihost(4, "AB_ARM_CIRCLE_CONTACTS_V2 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[38];
        for (unsigned shape = 0; shape < 2; ++shape) {
            size_t start = shape * 7;
            words[start] = random_float(&state, 125, 6, 0);
            words[start + 1] = random_float(&state, 120, 10, 1);
            words[start + 2] = random_float(&state, 120, 10, 1);
            words[start + 3] = random_float(&state, 114, 19, 1);
            words[start + 4] = random_float(&state, 114, 19, 1);
            words[start + 5] = rotations[(index + shape) % 4][0];
            words[start + 6] = rotations[(index + shape) % 4][1];
        }
        FloatBits first_point[] = {{.bits = words[1]}, {.bits = words[2]}};
        FloatBits second_point[] = {{.bits = words[8]}, {.bits = words[9]}};
        FloatBits center[2];
        transform_position(words + 3, first_point, center);
        const uint32_t rotation_only[] = {0, 0, words[12], words[13]};
        FloatBits rotated[2];
        transform_position(rotation_only, second_point, rotated);
        const float distances[] = {0, 0.5f, 1.0f, 0x1.000002p+0f, 2.0f};
        FloatBits radius = {.bits = add_float(words[0], words[7])};
        center[index & 1].number += radius.number * distances[index % 5];
        words[10] = ((FloatBits){.number = center[0].number - rotated[0].number}).bits;
        words[11] = ((FloatBits){.number = center[1].number - rotated[1].number}).bits;
        if (index < 8) {
            const uint32_t tiny_offsets[] = {0,          0x33800000, 0x33ffffff,
                                             0x34000000, 0x34000001, 0x34800000,
                                             0x34000000, 0x3f800000};
            for (size_t field = 0; field < 14; ++field) {
                words[field] = 0;
            }
            words[0] = 0x3f800000;
            words[6] = 0x3f800000;
            words[7] = 0x3f800000;
            words[13] = 0x3f800000;
            words[11] = tiny_offsets[index];
            words[10] = index == 6 ? 0x34000000u : 0;
        }
        const uint32_t first[] = {0, 0, words[0], words[1], words[2]};
        const uint32_t second[] = {0, 0, words[7], words[8], words[9]};
        initial_manifold(words + 14, index);
        collide(words + 14, first, words + 3, second, words + 10);
        record_world(initialize_world, words + 30, words + 14, words + 3, words[0],
                     words + 10, words[7], index);
        print_words(words, 38);
    }
}

static uint32_t normal_component(int value, int diagonal) {
    return value == 0
               ? 0
               : (value < 0 ? 0x80000000u : 0) | (diagonal ? 0x3f3504f3u : 0x3f800000u);
}

static void polygon_vertices(uint32_t shape[38], size_t index, uint32_t *state,
                             FloatBits *scale, FloatBits offset[2]) {
    const int8_t vertices[][16] = {
        {-1, -1, 1, -1, -1, 1},
        {-2, 0, -1, -1, 1, -1, 2, 0, 1, 1, -1, 1},
        {-2, -1, -1, -2, 1, -2, 2, -1, 2, 1, 1, 2, -1, 2, -2, 1},
    };
    const int8_t normals[][16] = {
        {0, -1, 1, 1, -1, 0},
        {-1, -1, 0, -1, 1, -1, 1, 1, 0, 1, -1, 1},
        {-1, -1, 0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0},
    };
    const uint32_t counts[] = {3, 6, 8};
    size_t kind = index % 3;
    uint32_t count = counts[kind];
    shape[37] = count;
    scale->bits = random_float(state, 119, 15, 0);
    offset[0].bits = random_float(state, 114, 20, 1);
    offset[1].bits = random_float(state, 114, 20, 1);
    for (uint32_t vertex = 0; vertex < count; ++vertex) {
        for (unsigned axis = 0; axis < 2; ++axis) {
            FloatBits value = {.number =
                                   offset[axis].number +
                                   scale->number * vertices[kind][vertex * 2 + axis]};
            shape[5 + vertex * 2 + axis] = value.bits;
            shape[21 + vertex * 2 + axis] = normal_component(
                normals[kind][vertex * 2 + axis],
                normals[kind][vertex * 2] && normals[kind][vertex * 2 + 1]);
        }
    }
}

static void probe_polygons(void) {
    typedef void (*SetBox)(void *, float, float);
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    ShapeMass mass =
        (ShapeMass)find_function("_ZNK14b2PolygonShape11ComputeMassEP10b2MassDataf");
    ShapeBounds bounds = (ShapeBounds)find_function(
        "_ZNK14b2PolygonShape11ComputeAABBEP6b2AABBRK11b2Transformi");
    ShapeContains contains = (ShapeContains)find_function(
        "_ZNK14b2PolygonShape9TestPointERK11b2TransformRK6b2Vec2");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    uint32_t state = 0x52090179;
    semihost(4, "AB_ARM_POLYGONS_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[50] = {0};
        uint32_t shape[38] = {0};
        shape[2] = index % 2 ? 0x3c23d70au : 0;
        FloatBits scale = {0};
        FloatBits offset[2] = {0};
        if (index < 2048) {
            FloatBits half_width = {.bits = random_float(&state, 115, 23, 0)};
            FloatBits half_height = {.bits = random_float(&state, 115, 23, 0)};
            set_box(shape, half_width.number, half_height.number);
            scale = half_width;
        } else {
            polygon_vertices(shape, index, &state, &scale, offset);
        }
        uint32_t count = shape[37];
        words[0] = count;
        words[1] = shape[2];
        words[2] = random_float(&state, 115, 18, 0);
        words[3] = random_float(&state, 117, 16, 1);
        words[4] = random_float(&state, 117, 16, 1);
        words[5] = rotations[index % 4][0];
        words[6] = rotations[index % 4][1];
        words[7] = random_float(&state, 117, 24, 1);
        words[8] = random_float(&state, 117, 24, 1);
        if (index % 4) {
            FloatBits sine = {.bits = words[5]}, cosine = {.bits = words[6]};
            FloatBits x = {.number = -(sine.number * offset[1].number -
                                       cosine.number * offset[0].number)};
            FloatBits y = {.number = cosine.number * offset[1].number +
                                     sine.number * offset[0].number};
            words[7] = add_float(x.bits, words[3]);
            words[8] = add_float(y.bits, words[4]);
            if (index % 4 == 3) {
                scale.number = scale.number * 8;
                words[7] = add_float(words[7], scale.bits);
            }
        }
        for (uint32_t vertex = 0; vertex < count * 2; ++vertex) {
            words[9 + vertex] = shape[5 + vertex];
            words[9 + count * 2 + vertex] = shape[21 + vertex];
        }
        uint32_t result = 9 + count * 4;
        FloatBits density = {.bits = words[2]};
        mass(shape, words + result, density.number);
        bounds(shape, words + result + 4, words + 3, 0);
        words[result + 8] = contains(shape, words + 3, words + 7);
        print_words(words, result + 9);
    }
}

static void probe_polygon_circle_contacts(void) {
    typedef void (*SetBox)(void *, float, float);
    typedef void (*Collide)(void *, const void *, const void *, const void *,
                            const void *);
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    Collide collide =
        (Collide)find_function("_Z25b2CollidePolygonAndCircleP10b2ManifoldPK14b2Polygon"
                               "ShapeRK11b2TransformPK13b2CircleShapeS6_");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    WorldQuery initialize_world = world_function();
    uint32_t state = 0x520c0112;
    semihost(4, "AB_ARM_POLYGON_CIRCLE_CONTACTS_V2 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[69] = {0};
        uint32_t polygon[38] = {0};
        polygon[2] = index & 1 ? 0x3c23d70au : 0;
        FloatBits scale = {0}, offset[2] = {0};
        if (index < 2048) {
            set_box(polygon, 2.0f, 3.0f);
            scale.number = 2;
        } else {
            polygon_vertices(polygon, index, &state, &scale, offset);
        }
        uint32_t count = polygon[37];
        words[0] = count;
        words[1] = polygon[2];
        words[2] = random_float(&state, 114, 19, 1);
        words[3] = random_float(&state, 114, 19, 1);
        words[4] = rotations[index % 4][0];
        words[5] = rotations[index % 4][1];
        words[6] = random_float(&state, 124, 7, 0);
        words[7] = random_float(&state, 120, 10, 1);
        words[8] = random_float(&state, 120, 10, 1);
        words[11] = rotations[(index + 1) % 4][0];
        words[12] = rotations[(index + 1) % 4][1];
        FloatBits local[2] = {offset[0], offset[1]};
        if (index < 2048) {
            const float locations[][2] = {
                {0, 0},         {2, 0},       {2.5f, 0},        {2.50000024f, 0},
                {2.25f, 3.25f}, {2.5f, 3.5f}, {-2.25f, -3.25f}, {0, -3.25f},
                {-2.5f, 0},     {3, 4},
            };
            local[0].number = locations[index % 10][0];
            local[1].number = locations[index % 10][1];
            words[6] = 0x3f000000;
        } else if (index % 3) {
            local[index & 1].number += scale.number * (float)(index % 5);
        }
        FloatBits world[2], rotated[2];
        transform_position(words + 2, local, world);
        FloatBits circle_local[] = {{.bits = words[7]}, {.bits = words[8]}};
        const uint32_t rotation_only[] = {0, 0, words[11], words[12]};
        transform_position(rotation_only, circle_local, rotated);
        words[9] = ((FloatBits){.number = world[0].number - rotated[0].number}).bits;
        words[10] = ((FloatBits){.number = world[1].number - rotated[1].number}).bits;
        for (uint32_t vertex = 0; vertex < count * 2; ++vertex) {
            words[13 + vertex] = polygon[5 + vertex];
            words[13 + count * 2 + vertex] = polygon[21 + vertex];
        }
        uint32_t result = 13 + count * 4;
        const uint32_t circle[] = {0, 0, words[6], words[7], words[8]};
        initial_manifold(words + result, index);
        collide(words + result, polygon, words + 2, circle, words + 9);
        record_world(initialize_world, words + result + 16, words + result, words + 2,
                     words[1], words + 9, words[6], index);
        print_words(words, result + 24);
    }
}

static void probe_polygon_contacts(void) {
    typedef void (*SetBox)(void *, float, float);
    typedef void (*Collide)(void *, const void *, const void *, const void *,
                            const void *);
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    Collide collide = (Collide)find_function(
        "_Z17b2CollidePolygonsP10b2ManifoldPK14b2PolygonShapeRK11b2TransformS3_S6_");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    WorldQuery initialize_world = world_function();
    uint32_t state = 0x520c0113;
    semihost(4, "AB_ARM_POLYGON_CONTACTS_V2 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[100] = {0};
        uint32_t first[38] = {0}, second[38] = {0};
        first[2] = index & 1 ? 0x3c23d70au : 0;
        second[2] = index & 2 ? 0x3c23d70au : 0;
        FloatBits first_scale = {0}, second_scale = {0};
        FloatBits first_offset[2] = {0}, second_offset[2] = {0};
        if (index < 2048) {
            set_box(first, 2, 3);
            set_box(second, 1.5f, 2);
            first_scale.number = 2;
            second_scale.number = 1.5f;
        } else {
            polygon_vertices(first, index, &state, &first_scale, first_offset);
            polygon_vertices(second, index / 3, &state, &second_scale, second_offset);
        }
        words[0] = first[37];
        words[1] = second[37];
        words[2] = first[2];
        words[3] = second[2];
        words[4] = random_float(&state, 114, 19, 1);
        words[5] = random_float(&state, 114, 19, 1);
        words[6] = rotations[index % 4][0];
        words[7] = rotations[index % 4][1];
        words[10] = rotations[(index / 4) % 4][0];
        words[11] = rotations[(index / 4) % 4][1];
        FloatBits center[2], rotated[2];
        transform_position(words + 4, first_offset, center);
        const uint32_t rotation_only[] = {0, 0, words[10], words[11]};
        transform_position(rotation_only, second_offset, rotated);
        const float distances[] = {0, 0.5f, 1, 1.0001f, 2, 3, -0.5f, -1, -1.0001f, -2};
        float distance = first_scale.number + second_scale.number;
        center[index & 1].number += distance * distances[index % 10];
        words[8] = ((FloatBits){.number = center[0].number - rotated[0].number}).bits;
        words[9] = ((FloatBits){.number = center[1].number - rotated[1].number}).bits;
        size_t second_start = 12 + (size_t)first[37] * 4;
        for (uint32_t vertex = 0; vertex < first[37] * 2; ++vertex) {
            words[12 + vertex] = first[5 + vertex];
            words[12 + first[37] * 2 + vertex] = first[21 + vertex];
        }
        for (uint32_t vertex = 0; vertex < second[37] * 2; ++vertex) {
            words[second_start + vertex] = second[5 + vertex];
            words[second_start + second[37] * 2 + vertex] = second[21 + vertex];
        }
        size_t result = second_start + (size_t)second[37] * 4;
        initial_manifold(words + result, index);
        collide(words + result, first, words + 4, second, words + 8);
        record_world(initialize_world, words + result + 16, words + result, words + 4,
                     words[2], words + 8, words[3], index);
        print_words(words, result + 24);
    }
}

static void probe_polygon_build(void) {
    typedef void (*SetPolygon)(void *, const void *, int);
    SetPolygon set_polygon =
        (SetPolygon)find_function("_ZN14b2PolygonShape3SetEPK6b2Vec2i");
    uint32_t state = 0x520b011d;
    semihost(4, "AB_ARM_POLYGON_BUILD_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[61] = {0};
        uint32_t shape[38] = {0};
        uint32_t count = (uint32_t)(index % 13);
        words[0] = count;
        words[1] = index & 1 ? 0x3c23d70au : 0;
        shape[2] = words[1];
        for (uint32_t vertex = 0; vertex < count; ++vertex) {
            words[2 + vertex * 2] = random_float(&state, 122, 12, 1);
            words[3 + vertex * 2] = random_float(&state, 122, 12, 1);
            if (index % 7 == 0) {
                words[3 + vertex * 2] = 0x3f800000;
            } else if (index % 7 == 1 && vertex % 2) {
                words[2 + vertex * 2] = words[2];
                words[3 + vertex * 2] = words[3];
            } else if (index % 7 == 2) {
                words[2 + vertex * 2] = vertex & 1 ? 0x3f800000u : 0xbf800000u;
                words[3 + vertex * 2] = vertex & 2 ? 0x3f800000u : 0xbf800000u;
            }
        }
        if (index < 16) {
            count = 3;
            words[0] = count;
            const uint32_t widths[] = {0,          0x3e000000, 0x3e200000, 0x3e22d0e5,
                                       0x3e800000, 0x3f800000, 0xbf800000, 0x80000000};
            words[2] = 0;
            words[3] = 0;
            words[4] = widths[index % 8];
            words[5] = 0;
            words[6] = 0;
            words[7] = 0x3f800000;
            if (index >= 8) {
                uint32_t swap = words[4];
                words[4] = words[6];
                words[6] = swap;
                swap = words[5];
                words[5] = words[7];
                words[7] = swap;
            }
        }
        set_polygon(shape, words + 2, (int)count);
        if (shape[2] != words[1]) {
            fail("Polygon build unexpectedly changed the skin radius\n");
        }
        uint32_t output = 2 + count * 2;
        words[output] = shape[37];
        words[output + 1] = shape[3];
        words[output + 2] = shape[4];
        for (uint32_t vertex = 0; vertex < shape[37] * 2; ++vertex) {
            words[output + 3 + vertex] = shape[5 + vertex];
            words[output + 3 + shape[37] * 2 + vertex] = shape[21 + vertex];
        }
        print_words(words, output + 3 + shape[37] * 4);
    }
}

static void ray_endpoints(uint32_t output[5], const uint32_t transform[4],
                          const FloatBits center[2], float scale, size_t index) {
    const float segments[][4] = {
        {-2, 0, 2, 0},  {0, 0, 2, 0},  {-2, 1, 2, 1}, {-2, 2, 2, 2},
        {-1, 0, -2, 0}, {-1, 0, 1, 0}, {2, 0, -2, 0}, {-2, 0, -2, 0},
    };
    const uint32_t fractions[] = {0, 0x3e800000, 0x3f000000, 0x3f800000, 0x40000000};
    for (size_t end = 0; end < 2; ++end) {
        FloatBits local[] = {
            {.number = center[0].number + scale * segments[index % 8][end * 2]},
            {.number = center[1].number + scale * segments[index % 8][end * 2 + 1]},
        };
        FloatBits world[2];
        transform_position(transform, local, world);
        output[end * 2] = world[0].bits;
        output[end * 2 + 1] = world[1].bits;
    }
    output[4] = fractions[(index / 8) % 5];
}

static void initial_ray_output(uint32_t output[3], size_t index) {
    for (uint32_t field = 0; field < 3; ++field) {
        output[field] = 0x3d000000u + (field << 12) + ((uint32_t)index & 255);
    }
}

static void probe_circle_rays(void) {
    typedef uint32_t (*RayQuery)(const void *, void *, const void *, const void *, int);
    RayQuery raycast =
        (RayQuery)find_function("_ZNK13b2CircleShape7RayCastEP15b2RayCastOutputRK14b2Ra"
                                "yCastInputRK11b2Transformi");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    uint32_t state = 0x5202a701;
    semihost(4, "AB_ARM_CIRCLE_RAYS_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[16];
        words[0] = random_float(&state, 124, 8, 0);
        words[1] = random_float(&state, 118, 12, 1);
        words[2] = random_float(&state, 118, 12, 1);
        words[3] = random_float(&state, 114, 19, 1);
        words[4] = random_float(&state, 114, 19, 1);
        words[5] = rotations[index % 4][0];
        words[6] = rotations[index % 4][1];
        FloatBits radius = {.bits = words[0]};
        FloatBits center[] = {{.bits = words[1]}, {.bits = words[2]}};
        ray_endpoints(words + 7, words + 3, center, radius.number, index);
        if (index < 8) {
            const float lengths[] = {0,         0x1p-23f,  0.0001f, 0.0002f,
                                     0.000345f, 0.000346f, 0.001f,  1};
            for (size_t field = 0; field < 12; ++field) {
                words[field] = 0;
            }
            words[0] = 0x3f800000;
            words[6] = 0x3f800000;
            words[7] = 0xbf800000;
            words[9] = ((FloatBits){.number = -1.0f + lengths[index]}).bits;
            words[11] = 0x3f800000;
        }
        const uint32_t shape[] = {0, 0, words[0], words[1], words[2]};
        initial_ray_output(words + 13, index);
        words[12] = raycast(shape, words + 13, words + 7, words + 3, 0);
        print_words(words, 16);
    }
}

static void probe_polygon_rays(void) {
    typedef void (*SetBox)(void *, float, float);
    typedef uint32_t (*RayQuery)(const void *, void *, const void *, const void *, int);
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    RayQuery raycast =
        (RayQuery)find_function("_ZNK14b2PolygonShape7RayCastEP15b2RayCastOutputRK14b2R"
                                "ayCastInputRK11b2Transformi");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    uint32_t state = 0x5202a702;
    semihost(4, "AB_ARM_POLYGON_RAYS_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[47] = {0};
        uint32_t shape[38] = {0};
        FloatBits scale = {0}, center[2] = {0};
        if (index < 2048) {
            set_box(shape, 2, 3);
            scale.number = 2;
        } else {
            polygon_vertices(shape, index, &state, &scale, center);
        }
        shape[2] = index & 1 ? 0x3c23d70au : 0;
        words[0] = shape[37];
        words[1] = shape[2];
        words[2] = random_float(&state, 114, 19, 1);
        words[3] = random_float(&state, 114, 19, 1);
        words[4] = rotations[index % 4][0];
        words[5] = rotations[index % 4][1];
        ray_endpoints(words + 6, words + 2, center, scale.number, index);
        for (uint32_t vertex = 0; vertex < shape[37] * 2; ++vertex) {
            words[11 + vertex] = shape[5 + vertex];
            words[11 + shape[37] * 2 + vertex] = shape[21 + vertex];
        }
        size_t result = 11 + (size_t)shape[37] * 4;
        initial_ray_output(words + result + 1, index);
        words[result] = raycast(shape, words + result + 1, words + 6, words + 2, 0);
        print_words(words, result + 4);
    }
}

static void probe_body_mass(void) {
    typedef void (*SetMass)(void *, const void *);
    SetMass set_mass = (SetMass)find_function("_ZN6b2Body11SetMassDataEPK10b2MassData");
    uint32_t world[0x191d4 / 4 + 1] = {0};
    uint32_t state = 0x520b0d11;
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    const uint32_t masses[] = {0,          0xbf800000, 0x3f000000,
                               0x3f800000, 0x40000000, 0x41000000};
    semihost(4, "AB_ARM_BODY_MASS_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[36] = {0};
        uint32_t body[64] = {0};
        words[0] = index % 5 < 3 ? 2u : index % 5 == 3 ? 0u : 1u;
        words[1] = index % 3 == 0 ? 1u : 0u;
        words[2] = index % 7 == 0 ? 1u : 0u;
        words[3] = masses[(index / 3) % 6];
        for (size_t field = 4; field < 24; ++field) {
            words[field] = random_float(&state, 115, 15, 1);
        }
        words[6] = index % 9 == 0 ? 0 : random_float(&state, 120, 18, index % 9 == 1);
        words[9] = rotations[index % 4][0];
        words[10] = rotations[index % 4][1];
        if (index == 0) {
            words[0] = 2;
            words[1] = 0;
            words[2] = 0;
            words[3] = 0x3f800000;
            words[4] = 0x3f800000;
            words[5] = 0;
            words[6] = 0x3f800000;
        }
        body[0] = words[0];
        body[1] = 0xau | words[1] << 4;
        body[22] = (uint32_t)(uintptr_t)world;
        world[0x191d4 / 4] = words[2] ? 2u : 0u;
        for (size_t field = 0; field < 10; ++field) {
            body[3 + field] = words[7 + field];
        }
        body[16] = words[17];
        body[17] = words[18];
        body[18] = words[19];
        for (size_t field = 0; field < 4; ++field) {
            body[29 + field] = words[20 + field];
        }
        set_mass(body, words + 3);
        if (body[0] != words[0] || body[1] != (0xau | words[1] << 4) ||
            body[18] != words[19]) {
            fail("Body mass override changed a preserved control field\n");
        }
        for (size_t field = 0; field < 4; ++field) {
            if (body[3 + field] != words[7 + field]) {
                fail("Body mass override changed its body transform\n");
            }
            words[24 + field] = body[29 + field];
        }
        for (size_t field = 0; field < 6; ++field) {
            words[28 + field] = body[7 + field];
        }
        words[34] = body[16];
        words[35] = body[17];
        print_words(words, 36);
    }
}

static void probe_body_reset(void) {
    typedef void (*ResetMass)(void *);
    typedef void (*SetBox)(void *, float, float);
    ResetMass reset_mass = (ResetMass)find_function("_ZN6b2Body13ResetMassDataEv");
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    ShapeMass compute[2] = {
        (ShapeMass)find_function("_ZNK13b2CircleShape11ComputeMassEP10b2MassDataf"),
        (ShapeMass)find_function("_ZNK14b2PolygonShape11ComputeMassEP10b2MassDataf"),
    };
    uint32_t vtables[2][8];
    for (size_t kind = 0; kind < 2; ++kind) {
        for (size_t slot = 0; slot < 8; ++slot) {
            vtables[kind][slot] = (uint32_t)(uintptr_t)unresolved_import;
        }
        vtables[kind][7] = (uint32_t)(uintptr_t)compute[kind];
    }
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},
        {0x3f800000, 0},
        {0x3f19999a, 0x3f4ccccd},
        {0xbf3504f3, 0xbf3504f3},
    };
    const uint32_t densities[] = {0,          0x80000000, 0x3f000000,
                                  0x3f800000, 0x40000000, 0xbf800000};
    uint32_t state = 0x520b0d12;
    semihost(4, "AB_ARM_BODY_RESET_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[56] = {0};
        uint32_t body[64] = {0};
        uint32_t nodes[4][4] = {0};
        uint32_t shapes[4][38] = {0};
        size_t type_case = (index / 5) % 5;
        words[0] = type_case < 3 ? 2u : type_case == 3 ? 0u : 1u;
        words[1] = index % 3 == 0 ? 1u : 0u;
        words[2] = (uint32_t)(index % 5);
        for (size_t field = 3; field < 22; ++field) {
            words[field] = random_float(&state, 115, 15, 1);
        }
        words[5] = rotations[index % 4][0];
        words[6] = rotations[index % 4][1];
        body[0] = words[0];
        body[1] = 0xau | words[1] << 4;
        for (size_t field = 0; field < 10; ++field) {
            body[3 + field] = words[3 + field];
        }
        body[13] = words[20];
        body[14] = words[21];
        body[16] = words[13];
        body[17] = words[14];
        body[18] = words[15];
        body[22] = UINT32_C(0xdead0000);
        for (size_t field = 0; field < 4; ++field) {
            body[29 + field] = words[16 + field];
        }
        for (size_t fixture = 0; fixture < words[2]; ++fixture) {
            size_t start = 22 + fixture * 5;
            unsigned kind = (unsigned)((index + fixture) % 2);
            FloatBits density = {.bits = densities[(index + fixture * 2) % 6]};
            words[start] = density.bits;
            nodes[fixture][0] = density.bits;
            nodes[fixture][1] =
                fixture + 1 < words[2] ? (uint32_t)(uintptr_t)nodes[fixture + 1] : 0;
            if (!(density.bits & UINT32_C(0x7fffffff))) {
                nodes[fixture][3] = UINT32_C(0xdead0000);
                for (size_t field = 1; field < 5; ++field) {
                    words[start + field] = UINT32_C(0x7fc00000);
                }
                continue;
            }
            if (kind) {
                if (index & 1) {
                    set_box(shapes[fixture], 2, 3);
                } else {
                    FloatBits scale = {0}, offset[2] = {0};
                    polygon_vertices(shapes[fixture], index + fixture, &state, &scale,
                                     offset);
                }
            } else {
                shapes[fixture][2] = random_float(&state, 115, 15, 0);
                shapes[fixture][3] = random_float(&state, 115, 15, 1);
                shapes[fixture][4] = random_float(&state, 115, 15, 1);
            }
            shapes[fixture][0] = (uint32_t)(uintptr_t)vtables[kind];
            nodes[fixture][3] = (uint32_t)(uintptr_t)shapes[fixture];
            compute[kind](shapes[fixture], words + start + 1, density.number);
        }
        body[25] = words[2] ? (uint32_t)(uintptr_t)nodes : 0;
        if (words[0] != 2) {
            body[25] = UINT32_C(0xdead0000);
        }
        reset_mass(body);
        if (body[0] != words[0] || body[1] != (0xau | words[1] << 4) ||
            body[18] != words[15]) {
            fail("Body mass reset changed a preserved control field\n");
        }
        size_t output = 22 + (size_t)words[2] * 5;
        for (size_t field = 0; field < 4; ++field) {
            if (body[3 + field] != words[3 + field]) {
                fail("Body mass reset changed its body transform\n");
            }
            words[output + field] = body[29 + field];
        }
        for (size_t field = 0; field < 6; ++field) {
            words[output + 4 + field] = body[7 + field];
        }
        words[output + 10] = body[16];
        words[output + 11] = body[17];
        words[output + 12] = body[13];
        words[output + 13] = body[14];
        print_words(words, output + 14);
    }
}

static void probe_warm_start(void) {
    typedef void (*WarmStart)(void *);
    WarmStart warm_start = (WarmStart)find_function("_ZN15b2ContactSolver9WarmStartEv");
    const uint32_t normals[][2] = {
        {0x3f800000, 0},          {0, 0x3f800000},          {0xbf800000, 0x80000000},
        {0x3f19999a, 0x3f4ccccd}, {0xbf3504f3, 0x3f3504f3},
    };
    uint32_t state = 0x5201a4e0;
    semihost(4, "AB_ARM_WARM_START_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[109] = {0};
        uint32_t velocities[12];
        uint32_t constraints[4][39] = {0};
        uint32_t solver[13] = {0};
        size_t count = index % 5;
        words[0] = (uint32_t)count;
        for (size_t field = 0; field < 12; ++field) {
            uint32_t bits = random_float(&state, 110, 27, 1);
            if (index % 8 == 0) {
                bits = field & 1 ? UINT32_C(0x80000000) : 0;
            }
            words[1 + field] = bits;
            velocities[field] = bits;
        }
        for (size_t contact = 0; contact < count; ++contact) {
            uint32_t *native = constraints[contact];
            size_t start = 13 + contact * 21;
            words[start] = (uint32_t)((index / 5 + contact) % 3);
            words[start + 1] = (uint32_t)((index / 15 + contact) % 4);
            words[start + 2] = (uint32_t)((index / 60 + contact * 3) % 4);
            native[37] = words[start];
            native[28] = words[start + 1];
            native[29] = words[start + 2];
            for (size_t field = 0; field < 4; ++field) {
                uint32_t bits = (index + contact + field) % 7 == 0
                                    ? 0
                                    : random_float(&state, 112, 17, 0);
                words[start + 3 + field] = bits;
                native[30 + field] = bits;
            }
            for (size_t axis = 0; axis < 2; ++axis) {
                uint32_t bits = normals[(index / 7 + contact) % 5][axis];
                words[start + 7 + axis] = bits;
                native[18 + axis] = bits;
            }
            for (size_t point = 0; point < 2; ++point) {
                for (size_t field = 0; field < 6; ++field) {
                    uint32_t bits =
                        random_float(&state, 110, 23, field < 4 || field == 5);
                    if (index % 8 == 0) {
                        bits = field & 1 ? UINT32_C(0x80000000) : 0;
                    }
                    words[start + 9 + point * 6 + field] = bits;
                    native[point * 9 + field] = bits;
                }
            }
        }
        solver[7] = (uint32_t)(uintptr_t)velocities;
        solver[10] = (uint32_t)(uintptr_t)constraints;
        solver[12] = (uint32_t)count;
        warm_start(solver);
        size_t output = 13 + count * 21;
        for (size_t field = 0; field < 12; ++field) {
            words[output + field] = velocities[field];
        }
        print_words(words, output + 12);
    }
}

static void probe_velocity_solver(void) {
    typedef void (*Solve)(void *);
    Solve solve =
        (Solve)find_function("_ZN15b2ContactSolver24SolveVelocityConstraintsEv");
    if (*(const uint8_t *)find_symbol("g_blockSolve", 1) != 1) {
        fail("Target coupled contact solve is not enabled\n");
    }
    const uint32_t normals[][2] = {
        {0x3f800000, 0},          {0, 0x3f800000},          {0xbf800000, 0x80000000},
        {0x3f19999a, 0x3f4ccccd}, {0xbf3504f3, 0x3f3504f3},
    };
    uint32_t state = 0x520501ae;
    semihost(4, "AB_ARM_VELOCITY_SOLVER_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[68] = {0};
        uint32_t velocities[12];
        uint32_t native[39] = {0};
        uint32_t solver[13] = {0};
        words[0] = (uint32_t)(1 + index % 10);
        for (size_t field = 0; field < 12; ++field) {
            words[1 + field] = random_float(&state, 115, 16, 1);
            velocities[field] = words[1 + field];
        }
        for (size_t point = 0; point < 2; ++point) {
            for (size_t field = 0; field < 9; ++field) {
                native[point * 9 + field] = random_float(
                    &state, 114, 14, field < 4 || field == 5 || field == 8);
            }
        }
        native[18] = normals[index % 5][0];
        native[19] = normals[index % 5][1];
        FloatBits diagonal_first = {.bits = random_float(&state, 125, 5, 0)};
        FloatBits diagonal_second = {.bits = random_float(&state, 125, 5, 0)};
        FloatBits off_diagonal = {.number =
                                      (diagonal_first.number < diagonal_second.number
                                           ? diagonal_first.number
                                           : diagonal_second.number) *
                                      (index & 1 ? 0.25f : -0.25f)};
        FloatBits determinant = {.number =
                                     diagonal_first.number * diagonal_second.number -
                                     off_diagonal.number * off_diagonal.number};
        FloatBits inverse = {.number = 1.0f / determinant.number};
        FloatBits value = {.number = diagonal_second.number * inverse.number};
        native[20] = value.bits;
        value.number = -(off_diagonal.number * inverse.number);
        native[21] = value.bits;
        native[22] = value.bits;
        value.number = diagonal_first.number * inverse.number;
        native[23] = value.bits;
        native[24] = diagonal_first.bits;
        native[25] = off_diagonal.bits;
        native[26] = off_diagonal.bits;
        native[27] = diagonal_second.bits;
        value.number = 1.0f / diagonal_first.number;
        native[6] = value.bits;
        value.number = 1.0f / diagonal_second.number;
        native[15] = value.bits;
        native[28] = (uint32_t)((index / 5) % 4);
        native[29] = (uint32_t)((index / 20) % 4);
        for (size_t field = 30; field < 34; ++field) {
            native[field] =
                (index + field) % 7 == 0 ? 0 : random_float(&state, 116, 12, 0);
        }
        native[34] = random_float(&state, 123, 5, 0);
        native[35] = UINT32_C(0x3f800000);
        native[36] = index % 4 == 0 ? 0 : random_float(&state, 115, 15, 1);
        native[37] = (uint32_t)(1 + (index / 3) % 2);
        if (index % 16 == 0) {
            for (size_t field = 0; field < 12; ++field) {
                words[1 + field] = field & 1 ? UINT32_C(0x80000000) : 0;
                velocities[field] = words[1 + field];
            }
            for (size_t point = 0; point < 2; ++point) {
                for (size_t field = 0; field < 6; ++field) {
                    native[point * 9 + field] = field & 1 ? UINT32_C(0x80000000) : 0;
                }
                native[point * 9 + 8] = 0;
            }
            native[36] = 0;
        }
        for (size_t field = 0; field < 39; ++field) {
            words[13 + field] = native[field];
        }
        solver[7] = (uint32_t)(uintptr_t)velocities;
        solver[10] = (uint32_t)(uintptr_t)native;
        solver[12] = 1;
        for (size_t iteration = 0; iteration < words[0]; ++iteration) {
            solve(solver);
        }
        for (size_t field = 0; field < 39; ++field) {
            if (field != 4 && field != 5 && field != 13 && field != 14 &&
                native[field] != words[13 + field]) {
                fail("Velocity solver changed a preserved constraint field\n");
            }
        }
        for (size_t field = 0; field < 12; ++field) {
            words[52 + field] = velocities[field];
        }
        words[64] = native[4];
        words[65] = native[5];
        words[66] = native[13];
        words[67] = native[14];
        print_words(words, 68);
    }
}

static void probe_velocity_initialize(void) {
    typedef void (*Initialize)(void *);
    Initialize initialize = (Initialize)find_function(
        "_ZN15b2ContactSolver29InitializeVelocityConstraintsEv");
    const uint32_t normals[][2] = {
        {0x3f800000, 0},          {0, 0x3f800000},          {0xbf800000, 0x80000000},
        {0x3f19999a, 0x3f4ccccd}, {0xbf3504f3, 0x3f3504f3},
    };
    uint32_t state = 0x5201a171;
    semihost(4, "AB_ARM_VELOCITY_INITIALIZE_V2 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[132] = {0};
        uint32_t original[32] = {0};
        uint32_t native[39];
        uint32_t solver[13] = {0};
        uint32_t contact = (uint32_t)(uintptr_t)original;
        for (size_t field = 0; field < 89; ++field) {
            words[field] = random_float(&state, 115, 15, 1);
        }
        oracle_reference_rotation(index, (unsigned)(index & 1), words + 2, words + 128,
                                  words + 129);
        oracle_reference_rotation((index + 37) % 4096, (unsigned)(index & 2), words + 5,
                                  words + 130, words + 131);
        uint32_t *position = words + 12;
        uint32_t *manifold = words + 34;
        uint32_t *velocity = words + 50;
        uint32_t kind = (uint32_t)(index % 3);
        uint32_t point_count = kind ? (uint32_t)(1 + (index / 3) % 2) : 1;
        manifold[14] = kind;
        manifold[15] = point_count;
        manifold[10] = normals[(index / 2) % 5][0];
        manifold[11] = normals[(index / 2) % 5][1];
        if (index % 5 == 0) {
            manifold[5] = manifold[0];
            manifold[6] = manifold[1];
        }
        for (size_t point = 0; point < 2; ++point) {
            position[point * 2] = manifold[point * 5];
            position[point * 2 + 1] = manifold[point * 5 + 1];
        }
        for (size_t field = 4; field < 8; ++field) {
            position[field] = manifold[6 + field];
        }
        position[8] = 0;
        position[9] = 1;
        position[18] = kind;
        position[21] = point_count;
        const size_t inverse_fields[] = {10, 11, 16, 17};
        for (size_t field = 0; field < 4; ++field) {
            uint32_t bits =
                (index + field) % 5 == 0 ? 0 : random_float(&state, 115, 14, 0);
            position[inverse_fields[field]] = bits;
            velocity[30 + field] = bits;
        }
        position[19] = random_float(&state, 115, 10, 0);
        position[20] = random_float(&state, 115, 10, 0);
        velocity[28] = 0;
        velocity[29] = 1;
        velocity[35] = random_float(&state, 120, 7, 0);
        velocity[37] = point_count;
        velocity[38] = 0;
        velocity[22] = velocity[21];
        velocity[26] = velocity[25];
        for (size_t field = 0; field < 16; ++field) {
            original[16 + field] = manifold[field];
        }
        for (size_t field = 0; field < 39; ++field) {
            native[field] = velocity[field];
        }
        solver[6] = (uint32_t)(uintptr_t)words;
        solver[7] = (uint32_t)(uintptr_t)(words + 6);
        solver[9] = (uint32_t)(uintptr_t)position;
        solver[10] = (uint32_t)(uintptr_t)native;
        solver[11] = (uint32_t)(uintptr_t)&contact;
        solver[12] = 1;
        oracle_math_enable(1);
        initialize(solver);
        oracle_math_enable(0);
        for (size_t field = 0; field < 39; ++field) {
            words[89 + field] = native[field];
        }
        print_words(words, 132);
    }
}

static void probe_position_point(void) {
    typedef uint32_t (*Solve)(void *);
    Solve solve =
        (Solve)find_function("_ZN15b2ContactSolver24SolvePositionConstraintsEv");
    const uint32_t normals[][2] = {
        {0x3f800000, 0},          {0, 0x3f800000},          {0xbf800000, 0x80000000},
        {0x3f19999a, 0x3f4ccccd}, {0xbf3504f3, 0x3f3504f3},
    };
    const uint32_t boundaries[] = {0,          0x80000000, 0xbd4ccccc, 0xbd4ccccd,
                                   0xbd4cccce, 0xbe199999, 0xbe19999a, 0xbe19999b,
                                   0xbf866665, 0xbf866666, 0xbf866667, 0x3d4ccccd};
    const uint32_t short_circles[] = {0,          0x80000000, 1,
                                      0x80000001, 0x33800000, 0x33ffffff,
                                      0x34000000, 0x34000001, 0x34800000};
    uint32_t state = UINT32_C(0x52000517);
    semihost(4, "AB_ARM_POSITION_POINT_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[39] = {0};
        uint32_t positions[6];
        uint32_t solver[13] = {0};
        for (size_t field = 0; field < 28; ++field) {
            words[field] = random_float(&state, 115, 15, 1);
        }
        oracle_reference_rotation(index, (unsigned)(index & 1), words + 2, words + 28,
                                  words + 29);
        oracle_reference_rotation((index + 37) % 4096, (unsigned)(index & 2), words + 5,
                                  words + 30, words + 31);
        uint32_t *constraint = words + 6;
        constraint[4] = normals[index % 5][0];
        constraint[5] = normals[index % 5][1];
        constraint[8] = 0;
        constraint[9] = 1;
        const size_t inverse_fields[] = {10, 11, 16, 17};
        for (size_t field = 0; field < 4; ++field) {
            constraint[inverse_fields[field]] =
                (index + field) % 7 == 0 ? 0 : random_float(&state, 115, 15, 0);
        }
        constraint[18] = (uint32_t)(index % 3);
        constraint[19] = random_float(&state, 115, 12, 0);
        constraint[20] = random_float(&state, 115, 12, 0);
        constraint[21] = 1;
        size_t boundary_count = sizeof(boundaries) / sizeof(boundaries[0]);
        size_t circle_count = sizeof(short_circles) / sizeof(short_circles[0]);
        if (index < boundary_count + circle_count) {
            for (size_t field = 0; field < 28; ++field) {
                words[field] = 0;
            }
            words[3] = index < boundary_count ? boundaries[index]
                                              : short_circles[index - boundary_count];
            constraint[4] = UINT32_C(0x3f800000);
            constraint[9] = 1;
            for (size_t field = 0; field < 4; ++field) {
                constraint[inverse_fields[field]] = UINT32_C(0x3f800000);
            }
            constraint[18] = index < boundary_count ? 1 : 0;
            if (index >= boundary_count) {
                constraint[19] = UINT32_C(0x3f000000);
                constraint[20] = UINT32_C(0x3f000000);
            }
            constraint[21] = 1;
            words[28] = 0;
            words[29] = UINT32_C(0x3f800000);
            words[30] = 0;
            words[31] = UINT32_C(0x3f800000);
        }
        for (size_t field = 0; field < 6; ++field) {
            positions[field] = words[field];
        }
        solver[6] = (uint32_t)(uintptr_t)positions;
        solver[9] = (uint32_t)(uintptr_t)constraint;
        solver[12] = 1;
        oracle_math_enable(1);
        words[38] = solve(solver);
        oracle_math_enable(0);
        for (size_t field = 0; field < 6; ++field) {
            words[32 + field] = positions[field];
        }
        print_words(words, 39);
    }
}

#if defined(AB_ORACLE_ANDROID)
#if defined(AB_ORACLE_ISLAND_ONLY)
#include "island_contacts.h"
#if defined(AB_ORACLE_ISLAND_REPORT)
#include "island_reports.h"
#endif

static void probe_islands(unsigned with_contacts) {
    typedef void (*ConstructAllocator)(void *);
    typedef void (*ConstructIsland)(void *, int, int, int, void *, void *);
    typedef void (*DestroyIsland)(void *);
    typedef void (*SolveIsland)(void *, void *, const void *, const void *, unsigned);
    typedef void (*SolveToiIsland)(void *, const void *, int, int);
    ConstructAllocator construct_allocator =
        (ConstructAllocator)find_function("_ZN16b2StackAllocatorC1Ev");
    ConstructIsland construct_island = (ConstructIsland)find_function(
        "_ZN8b2IslandC1EiiiP16b2StackAllocatorP17b2ContactListener");
    DestroyIsland destroy_island = (DestroyIsland)find_function("_ZN8b2IslandD1Ev");
    SolveIsland solve = (SolveIsland)find_function(
        "_ZN8b2Island5SolveEP9b2ProfileRK10b2TimeStepRK6b2Vec2b");
    SolveToiIsland solve_toi =
        (SolveToiIsland)find_function("_ZN8b2Island8SolveTOIERK10b2TimeStepii");
#if defined(AB_ORACLE_TOI_ISLAND)
    const unsigned continuous = 1;
#else
    const unsigned continuous = 0;
#endif
    const uint32_t *translation = (const uint32_t *)find_symbol("b2_maxTranslation", 1);
    const uint32_t *translation_squared =
        (const uint32_t *)find_symbol("b2_maxTranslationSquared", 1);
    static uint64_t allocator[16384];
    const uint32_t steps[] = {0, 0x3d088889, 0x3c23d70a, 0x3e800000, 0x3f800000};
    const uint32_t sleep_times[] = {0, 0x3e7fffff, 0x3e800000, 0x3f000000};
    const uint32_t linear_edges[] = {0, 0x3dcccccc, 0x3dcccccd, 0x3dccccce};
    const uint32_t angular_edges[] = {0, 0x3d0efa34, 0x3d0efa35, 0x3d0efa36};
    uint32_t state = UINT32_C(0x52015a1d);
#if defined(AB_ORACLE_ISLAND_REPORT)
    uint32_t report_vtable[] = {0, 0, 0,
                                0, 0, (uint32_t)(uintptr_t)record_island_report};
    uint32_t listener[] = {(uint32_t)(uintptr_t)report_vtable};
    semihost(4, continuous ? "AB_ANDROID_TOI_ISLAND_V1 4096\n"
                           : "AB_ANDROID_CONTACT_REPORT_V1 4096\n");
#else
    void *listener = NULL;
    semihost(4, with_contacts ? "AB_ANDROID_CONTACT_ISLAND_V1 4096\n"
                              : "AB_ANDROID_FREE_ISLAND_V1 4096\n");
#endif
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t controls[14] = {
            (uint32_t)(with_contacts ? 2 + index % 3 : index % 5),
            steps[(index / 5) % 5],
            random_float(&state, 120, 11, 1),
            random_float(&state, 120, 11, 1),
            (uint32_t)(index % 7 != 0),
            (uint32_t)(index % 3 ? 10 : 0),
            (uint32_t)(1 + index % 10),
            *translation,
            *translation_squared,
            (uint32_t)(with_contacts ? index % 5 : 0),
            (uint32_t)(index % 2),
            (const uint32_t[]){0, 0x3f000000, 0x3f800000, 0x40000000}[(index / 2) % 4],
        };
        uint32_t bodies[4][64] = {0};
        OracleIslandContact contacts[4] = {0};
        uint32_t island[13] = {0}, profile[8] = {0};
        uint32_t timestep[6] = {controls[1], 0,           0x3f800000,
                                controls[6], controls[5], 1};
        if (with_contacts) {
            timestep[2] = controls[11];
            timestep[5] = controls[10];
        }
        if (index % 4 == 1 || index % 4 == 2) {
            controls[2] = 0;
            controls[3] = 0;
        }
        if (continuous) {
            controls[12] = (uint32_t)((index / 11) % controls[0]);
            controls[13] = (uint32_t)((index / 43) % controls[0]);
        }
        print_words(controls, continuous ? 14 : with_contacts ? 12 : 9);
        construct_allocator(allocator);
        construct_island(island, 4, with_contacts ? 4 : 0, 0, allocator, listener);
        uint32_t *body_pointers = (uint32_t *)(uintptr_t)island[2];
        island[7] = controls[0];
        for (size_t body = 0; body < controls[0]; ++body) {
            uint32_t *current = bodies[body];
            current[0] = (uint32_t)((index + body) % 3);
            current[1] = UINT32_C(0xa5000000) | 0x2au;
            if (index % 4 != 2 || body || index % 8) {
                current[1] |= 4;
            }
            current[2] = (uint32_t)body;
            for (size_t field = 3; field < 22; ++field) {
                current[field] = random_float(&state, 115, 15, 1);
            }
            current[22] = UINT32_C(0xdead0000);
            for (size_t field = 29; field <= 36; ++field) {
                current[field] = random_float(&state, 118, 12, 0);
            }
            current[14] = random_float(&state, 118, 12, 1);
            current[30] = current[0] == 2 ? current[30] : 0;
            current[32] = current[0] == 2 && index % 3 ? current[32] : 0;
            current[35] = index % 11 ? random_float(&state, 123, 6, 1) : 0;
            if (!current[0] || index % 4 == 1 || index % 4 == 2) {
                for (size_t field = 16; field < 22; ++field) {
                    current[field] = 0;
                }
            }
            if (index % 4 == 2 && body == 0 && current[0]) {
                current[16] = linear_edges[(index / 4) % 4];
                current[18] = angular_edges[(index / 16) % 4];
            }
            if (index % 4 == 3 && current[0]) {
                current[16] = random_float(&state, 132, 5, 1);
                current[17] = random_float(&state, 132, 5, 1);
                current[18] = random_float(&state, 132, 5, 1);
            }
            current[36] = sleep_times[(index / 7 + body) % 4];
            body_pointers[body] = (uint32_t)(uintptr_t)current;
            print_words(current, 64);
        }
        uint32_t *contact_pointers = (uint32_t *)(uintptr_t)island[3];
        island[9] = controls[9];
        for (size_t contact = 0; contact < controls[9]; ++contact) {
            prepare_island_contact(contacts + contact, body_pointers, controls[0],
                                   index, contact, &state);
            contact_pointers[contact] = (uint32_t)(uintptr_t)contacts[contact].native;
            print_words(contacts[contact].data, 23);
        }
#if defined(AB_ORACLE_ISLAND_REPORT)
        begin_island_reports(body_pointers, controls[0], contact_pointers, controls[9]);
#endif
        if (continuous) {
            solve_toi(island, timestep, (int)controls[12], (int)controls[13]);
        } else {
            solve(island, profile, timestep, controls + 2, controls[4]);
        }
        for (size_t body = 0; body < controls[0]; ++body) {
            print_words(bodies[body], 64);
        }
        for (size_t contact = 0; contact < controls[9]; ++contact) {
            print_words(contacts[contact].native + 16, 16);
        }
#if defined(AB_ORACLE_ISLAND_REPORT)
        print_island_reports();
#endif
        destroy_island(island);
    }
#if defined(AB_ORACLE_ISLAND_REPORT)
    semihost(4, continuous ? "AB TOI island reference complete\n"
                           : "AB contact report reference complete\n");
#else
    semihost(4, with_contacts ? "AB contact island reference complete\n"
                              : "AB free island reference complete\n");
#endif
}
#endif

static void probe_position_passes(unsigned toi) {
    typedef uint32_t (*Solve)(void *);
    typedef uint32_t (*SolveToi)(void *, int, int);
    Solve solve =
        (Solve)find_function("_ZN15b2ContactSolver24SolvePositionConstraintsEv");
    SolveToi solve_toi =
        (SolveToi)find_function("_ZN15b2ContactSolver27SolveTOIPositionConstraintsEii");
    const uint32_t normals[][2] = {
        {0x3f800000, 0},          {0, 0x3f800000},          {0xbf800000, 0x80000000},
        {0x3f19999a, 0x3f4ccccd}, {0xbf3504f3, 0x3f3504f3},
    };
    uint32_t state = UINT32_C(0x520005a5);
    semihost(4, toi ? "AB_ANDROID_TOI_POSITION_V1 4096\n"
                    : "AB_ANDROID_POSITION_PASSES_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[234] = {0};
        uint32_t positions[12];
        uint32_t bodies[4][4];
        uint32_t constraints[4][22] = {0};
        uint32_t solver[13] = {0};
        words[0] = (uint32_t)(index % 5);
        words[1] = (uint32_t)(1 + (index / 5) % 10);
        size_t position_offset = toi ? 4 : 2;
        size_t constraint_offset = position_offset + 12;
        if (toi) {
            words[2] = (uint32_t)((index / 13) % 4);
            words[3] = (uint32_t)((index / 52) % 4);
        }
        for (size_t body = 0; body < 4; ++body) {
            for (size_t field = 0; field < 3; ++field) {
                uint32_t bits = random_float(&state, 115, 15, 1);
                if (field == 2) {
                    bits = random_float(&state, 122, 12, 1);
                }
                positions[body * 3 + field] = bits;
                words[position_offset + body * 3 + field] = bits;
            }
            bodies[body][0] =
                (index + body) % 5 == 0 ? 0 : random_float(&state, 124, 5, 0);
            bodies[body][1] = !bodies[body][0] || (index + body) % 3 == 0
                                  ? 0
                                  : random_float(&state, 124, 5, 0);
            bodies[body][2] = random_float(&state, 115, 14, 1);
            bodies[body][3] = random_float(&state, 115, 14, 1);
        }
        for (size_t contact = 0; contact < words[0]; ++contact) {
            uint32_t *current = constraints[contact];
            for (size_t field = 0; field < 8; ++field) {
                current[field] = random_float(&state, 115, 15, 1);
            }
            current[4] = normals[(index + contact) % 5][0];
            current[5] = normals[(index + contact) % 5][1];
            current[8] = (uint32_t)((index / 7 + contact) % 4);
            current[9] = (uint32_t)((index / 28 + contact * 3) % 4);
            current[10] = bodies[current[8]][0];
            current[11] = bodies[current[9]][0];
            current[12] = bodies[current[8]][2];
            current[13] = bodies[current[8]][3];
            current[14] = bodies[current[9]][2];
            current[15] = bodies[current[9]][3];
            current[16] = bodies[current[8]][1];
            current[17] = bodies[current[9]][1];
            current[18] = (uint32_t)((index / 3 + contact) % 3);
            current[19] = random_float(&state, 115, 12, 0);
            current[20] = random_float(&state, 115, 12, 0);
            current[21] = (uint32_t)((index / 11 + contact) % 3);
            if (!current[18] && current[21] == 2) {
                current[21] = 1;
            }
            for (size_t field = 0; field < 22; ++field) {
                words[constraint_offset + contact * 22 + field] = current[field];
            }
        }
        solver[6] = (uint32_t)(uintptr_t)positions;
        solver[9] = (uint32_t)(uintptr_t)constraints;
        solver[12] = words[0];
        size_t output = constraint_offset + (size_t)words[0] * 22;
        for (size_t iteration = 0; iteration < words[1]; ++iteration) {
            words[output + 12] =
                toi ? solve_toi(solver, (int)words[2], (int)words[3]) : solve(solver);
            for (size_t field = 0; field < 12; ++field) {
                words[output + field] = positions[field];
            }
            output += 13;
        }
        for (size_t contact = 0; contact < words[0]; ++contact) {
            for (size_t field = 0; field < 22; ++field) {
                if (constraints[contact][field] !=
                    words[constraint_offset + contact * 22 + field]) {
                    fail("Position solve changed a preserved constraint field\n");
                }
            }
        }
        print_words(words, output);
    }
    if (toi) {
        semihost(4, "AB TOI position reference complete\n");
    }
}
#endif

#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_TREE_ONLY)
#include "tree_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_BROADPHASE_ONLY)
#include "broadphase_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_BODY_SETUP_ONLY)
#include "body_setup_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_CONTACT_STATE_ONLY)
#include "contact_state_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_FIXTURE_ONLY)
#include "fixture_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_WORLD_SOLVE_ONLY)
#include "world_solve_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_DISTANCE_ONLY)
#include "distance_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_WORLD_STEPS_ONLY)
#include "world_steps_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_WORLD_RAYCAST_ONLY)
#include "world_raycast_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_SEPARATION_ONLY)
#include "separation_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_IMPACT_ONLY)
#include "impact_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_GAME_COLLISION_ONLY)
#include "game_collision_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_GAME_RULES_ONLY)
#include "game_rules_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_GAME_CONTACT_ROUTES_ONLY)
#include "game_contact_routes_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_GAME_CONTACT_DISPATCH_ONLY)
#include "game_contact_dispatch_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_STRING_ORDER_ONLY)
#include "string_order_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_GAME_ANGLE_ONLY)
#include "angle_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) &&                                                      \
    (defined(AB_ORACLE_BLOCK_DAMAGE_ONLY) || defined(AB_ORACLE_BIRD_DAMAGE_ONLY) ||    \
     defined(AB_ORACLE_BODY_VELOCITY_ONLY))
#include "block_damage_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_LEVEL_ORDER_ONLY)
#include "level_order_probe.h"
#endif

#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_MP3_ONLY)
#include "mp3_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_MP3_GAIN_ONLY)
#include "mp3_gain_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) &&                                                      \
    (defined(AB_ORACLE_MP3_DCT36_ONLY) || defined(AB_ORACLE_MP3_DCT12_ONLY))
#include "mp3_transform_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_MP3_WINDOWS_ONLY)
#include "mp3_windows_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_JOINT_GEOMETRY_ONLY)
#include "joint_geometry_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_MATRIX_INVERSE_ONLY)
#include "matrix_inverse_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_MATRIX_SOLVE_ONLY)
#include "matrix_solve_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_WELD_INITIALIZE_ONLY)
#include "weld_initialize_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_DISTANCE_JOINT_ONLY)
#include "distance_joint_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_REVOLUTE_INITIALIZE_ONLY)
#include "revolute_initialize_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_PRISMATIC_INITIALIZE_ONLY)
#include "prismatic_initialize_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_JOINT_ISLAND_ONLY)
#include "joint_island_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_WORLD_JOINT_ONLY)
#include "world_joint_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_MP3_SIDE_ONLY)
#include "mp3_side_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_AUDIO_MIXER_ONLY)
#include "audio_mixer_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_AUDIO_MIXER_STATE_ONLY)
#include "audio_mixer_state_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_TEXT_CATALOG_ONLY)
#include "text_catalog_probe.h"
#endif
#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_TEXT_LOCALE_ONLY)
#include "text_locale_probe.h"
#endif

#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_WORLD_CLEAR_ONLY)
#include "world_clear_probe.h"
#endif

#if defined(AB_ORACLE_ANDROID) && defined(AB_ORACLE_NATIVE_EVENTS_ONLY)
#include "native_events_probe.h"
#endif

void oracle_main(void) {
    initialize_target();
#if defined(AB_ORACLE_NATIVE_EVENTS_ONLY)
    probe_native_events();
    finish(0);
#endif
#if defined(AB_ORACLE_WORLD_CLEAR_ONLY)
    probe_world_clear();
    finish(0);
#endif
#if defined(AB_ORACLE_TEXT_LOCALE_ONLY)
    probe_text_locale();
    finish(0);
#endif
#if defined(AB_ORACLE_TEXT_CATALOG_ONLY)
    probe_text_catalog();
    finish(0);
#endif
#if defined(AB_ORACLE_AUDIO_MIXER_STATE_ONLY)
    probe_audio_mixer_state();
    finish(0);
#endif
#if defined(AB_ORACLE_AUDIO_MIXER_ONLY)
    probe_audio_mixer();
    finish(0);
#endif
#if defined(AB_ORACLE_MP3_ONLY)
    probe_mp3();
    finish(0);
#endif
#if defined(AB_ORACLE_MP3_GAIN_ONLY)
    probe_mp3_gain();
    finish(0);
#endif
#if defined(AB_ORACLE_MP3_DCT36_ONLY) || defined(AB_ORACLE_MP3_DCT12_ONLY)
    probe_mp3_transform();
    finish(0);
#endif
#if defined(AB_ORACLE_MP3_WINDOWS_ONLY)
    probe_mp3_windows();
    finish(0);
#endif
#if defined(AB_ORACLE_JOINT_GEOMETRY_ONLY)
    probe_joint_geometry();
    finish(0);
#endif
#if defined(AB_ORACLE_MATRIX_INVERSE_ONLY)
    probe_matrix_inverse();
    finish(0);
#endif
#if defined(AB_ORACLE_MATRIX_SOLVE_ONLY)
    probe_matrix_solve();
    finish(0);
#endif
#if defined(AB_ORACLE_WELD_INITIALIZE_ONLY)
    probe_weld_initialize();
    finish(0);
#endif
#if defined(AB_ORACLE_DISTANCE_JOINT_ONLY)
    probe_distance_joint();
    finish(0);
#endif
#if defined(AB_ORACLE_REVOLUTE_INITIALIZE_ONLY)
    probe_revolute_initialize();
    finish(0);
#endif
#if defined(AB_ORACLE_PRISMATIC_INITIALIZE_ONLY)
    probe_prismatic_initialize();
    finish(0);
#endif
#if defined(AB_ORACLE_JOINT_ISLAND_ONLY)
    probe_joint_island();
    finish(0);
#endif
#if defined(AB_ORACLE_WORLD_JOINT_ONLY)
    probe_world_joint();
    finish(0);
#endif
#if defined(AB_ORACLE_MP3_SIDE_ONLY)
    probe_mp3_side();
    finish(0);
#endif
#if defined(AB_ORACLE_LEVEL_ORDER_ONLY)
    probe_level_order();
    finish(0);
#endif
#if defined(AB_ORACLE_STRING_ORDER_ONLY)
    probe_string_order();
    finish(0);
#endif
#if defined(AB_ORACLE_GAME_ANGLE_ONLY)
    probe_game_angles();
    finish(0);
#endif
#if defined(AB_ORACLE_BLOCK_DAMAGE_ONLY) || defined(AB_ORACLE_BIRD_DAMAGE_ONLY) ||     \
    defined(AB_ORACLE_BODY_VELOCITY_ONLY)
    probe_block_damage();
    finish(0);
#endif
#if defined(AB_ORACLE_GAME_RULES_ONLY)
    probe_game_rules();
    finish(0);
#endif
#if defined(AB_ORACLE_GAME_CONTACT_ROUTES_ONLY)
    probe_game_contact_routes();
    finish(0);
#endif
#if defined(AB_ORACLE_GAME_CONTACT_DISPATCH_ONLY)
    probe_game_contact_dispatch();
    finish(0);
#endif
#if defined(AB_ORACLE_GAME_COLLISION_ONLY)
    probe_game_collision();
    finish(0);
#endif
#if defined(AB_ORACLE_TOI_POSITION_ONLY)
    probe_position_passes(1);
    finish(0);
#endif
#if defined(AB_ORACLE_IMPACT_ONLY)
    probe_impact();
    finish(0);
#endif
#if defined(AB_ORACLE_SEPARATION_ONLY)
    probe_separation();
    finish(0);
#endif
#if defined(AB_ORACLE_WORLD_STEPS_ONLY)
    probe_world_steps();
    finish(0);
#endif
#if defined(AB_ORACLE_WORLD_RAYCAST_ONLY)
    probe_world_raycast();
    finish(0);
#endif
#if defined(AB_ORACLE_DISTANCE_ONLY)
    probe_distance();
    finish(0);
#endif
#if defined(AB_ORACLE_WORLD_SOLVE_ONLY)
    probe_world_solve();
    finish(0);
#endif
#if defined(AB_ORACLE_FIXTURE_ONLY)
    probe_fixtures();
    finish(0);
#endif
#if defined(AB_ORACLE_CONTACT_STATE_ONLY)
    probe_contact_state();
    finish(0);
#endif
#if defined(AB_ORACLE_BODY_SETUP_ONLY)
    probe_body_setup();
    finish(0);
#endif
#if defined(AB_ORACLE_BROADPHASE_ONLY)
    probe_broadphase();
    finish(0);
#endif
#if defined(AB_ORACLE_TREE_ONLY)
    probe_tree();
    finish(0);
#endif
#if defined(AB_ORACLE_ISLAND_ONLY)
#if defined(AB_ORACLE_CONTACT_ISLAND)
    probe_islands(1);
#else
    probe_islands(0);
#endif
    finish(0);
#endif
    typedef void (*CircleMass)(const void *, void *, float);
    typedef void (*CircleBounds)(const void *, void *, const void *, int);
    typedef uint32_t (*CircleContains)(const void *, const void *, const void *);
    CircleMass mass =
        (CircleMass)find_function("_ZNK13b2CircleShape11ComputeMassEP10b2MassDataf");
    CircleBounds bounds = (CircleBounds)find_function(
        "_ZNK13b2CircleShape11ComputeAABBEP6b2AABBRK11b2Transformi");
    CircleContains contains = (CircleContains)find_function(
        "_ZNK13b2CircleShape9TestPointERK11b2TransformRK6b2Vec2");
    const uint32_t rotations[][2] = {
        {0, 0x3f800000},          {0x3f800000, 0},          {0, 0xbf800000},
        {0xbf800000, 0},          {0x3f19999a, 0x3f4ccccd}, {0xbf4ccccd, 0x3f19999a},
        {0x3f3504f3, 0x3f3504f3}, {0xbf3504f3, 0xbf3504f3},
    };
    const uint32_t edge_radii[] = {0,          0x80000000, 1,          0x007fffff,
                                   0x00800000, 0x3f800000, 0x40000000, 0x3f800001};
    uint32_t state = 0x520c1c1e;
    semihost(4, "AB_ARM_CIRCLES_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[19] = {0};
        words[0] = random_float(&state, 80, 60, 0);
        words[1] = random_float(&state, 110, 26, 1);
        words[2] = random_float(&state, 110, 26, 1);
        words[3] = random_float(&state, 115, 18, 0);
        words[4] = random_float(&state, 117, 24, 1);
        words[5] = random_float(&state, 117, 24, 1);
        words[6] = rotations[index % 8][0];
        words[7] = rotations[index % 8][1];
        words[8] = random_float(&state, 117, 24, 1);
        words[9] = random_float(&state, 117, 24, 1);
        if (index < 8) {
            words[0] = edge_radii[index];
            words[1] = index & 1 ? 0x80000000u : 0;
            words[2] = 0;
            words[3] = 0x40000000;
            words[4] = 0;
            words[5] = 0;
        }
        uint32_t circle[] = {0, 0, words[0], words[1], words[2]};
        union {
            float number;
            uint32_t bits;
        } density = {.bits = words[3]};
        mass(circle, words + 10, density.number);
        bounds(circle, words + 14, words + 4, 0);
        if (index % 4) {
            words[8] = add_float(words[14], words[0]);
            words[9] = add_float(words[15], words[0]);
            if (index % 4 == 2) {
                words[8] = add_float(words[8], words[0]);
            } else if (index % 4 == 3) {
                words[8] = add_float(add_float(words[8], words[0]), words[0]);
            }
        }
        words[18] = contains(circle, words + 4, words + 8);
        print_words(words, 19);
    }
    probe_polygons();
    probe_circle_contacts();
    probe_polygon_circle_contacts();
    probe_polygon_contacts();
    probe_polygon_build();
    probe_circle_rays();
    probe_polygon_rays();
    probe_body_mass();
    probe_body_reset();
    probe_warm_start();
    probe_velocity_solver();
    probe_velocity_initialize();
    probe_position_point();
#if defined(AB_ORACLE_ANDROID)
    probe_position_passes(0);
#endif
    finish(0);
}
