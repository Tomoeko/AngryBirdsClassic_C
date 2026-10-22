#ifndef AB_ORACLE_COORDINATES_H
#define AB_ORACLE_COORDINATES_H

static void probe_coordinates(void) {
    typedef void (*Transform)(FloatBits[2], const void *, float, float);
    typedef float (*Scalar)(const void *, float);
    const char *names[] = {"_ZNK20CoordinateTransforms23worldToPhysicsTransformEff",
                           "_ZNK20CoordinateTransforms23physicsToWorldTransformEff",
                           "_ZNK20CoordinateTransforms22screenToWorldTransformEff",
                           "_ZNK20CoordinateTransforms22worldToScreenTransformEff",
                           "_ZNK20CoordinateTransforms24physicsToScreenTransformEff",
                           "_ZNK20CoordinateTransforms24screenToPhysicsTransformEff",
                           "_ZNK20CoordinateTransforms22worldToPhysicsDistanceEff",
                           "_ZNK20CoordinateTransforms22physicsToWorldDistanceEff",
                           "_ZNK20CoordinateTransforms21screenToWorldDistanceEff",
                           "_ZNK20CoordinateTransforms21worldToScreenDistanceEff",
                           "_ZNK20CoordinateTransforms23physicsToScreenDistanceEff",
                           "_ZNK20CoordinateTransforms23screenToPhysicsDistanceEff"};
    Transform functions[12];
    for (size_t index = 0; index < 12; ++index) {
        functions[index] = (Transform)find_function(names[index]);
    }
    Scalar scalar = (Scalar)find_function(
        "_ZN20CoordinateTransforms28screenDistanceToPhysicsScaleEf");
    /* Only these fields are read by the selected nonvirtual methods. No Lua,
     * graphics context, or full camera/entity construction is required. */
    uint32_t camera[30] = {0};
    uint32_t coordinates[4] = {0, 0, 0, (uint32_t)(uintptr_t)camera};
    uint32_t random = UINT32_C(0x520c00ad);
    static const uint32_t boundaries[] = {0,
                                          UINT32_C(0x80000000),
                                          1,
                                          UINT32_C(0x80000001),
                                          UINT32_C(0x007fffff),
                                          UINT32_C(0x807fffff),
                                          UINT32_C(0x3f800000),
                                          UINT32_C(0xbf800000),
                                          UINT32_C(0x3f7fffff),
                                          UINT32_C(0x3f800001),
                                          UINT32_C(0x4b800000),
                                          UINT32_C(0xcb800000)};
    semihost(4, "AB_ANDROID_COORDINATES_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[7];
        for (size_t field = 0; field < 7; ++field) {
            input[field] = random_float(&random, 0, 145, 1);
        }
        input[2] = random_float(&random, 122, 10, 1);
        input[3] = random_float(&random, 122, 10, 1);
        if (item < 144) {
            input[4] = boundaries[item % 12];
            input[5] = boundaries[item / 12];
            input[6] = input[4];
        }
        if (item >= 144 && item < 288) {
            input[0] = UINT32_C(0x4c000000);
            input[1] = UINT32_C(0xcc000000);
            input[2] = input[3] = UINT32_C(0x3f800000);
            input[4] = boundaries[item % 12];
            input[5] = boundaries[(item - 144) / 12];
        }
        for (size_t field = 0; field < 4; ++field) {
            camera[26 + field] = input[field];
        }
        print_words(input, 7);
        uint32_t output[25];
        FloatBits x = {.bits = input[4]}, y = {.bits = input[5]};
        for (size_t operation = 0; operation < 12; ++operation) {
            FloatBits result[2] = {{.bits = UINT32_C(0x7fc00000)},
                                   {.bits = UINT32_C(0x7fc00000)}};
            functions[operation](result, coordinates, x.number, y.number);
            output[2 * operation] = result[0].bits;
            output[2 * operation + 1] = result[1].bits;
        }
        FloatBits distance = {.bits = input[6]};
        FloatBits converted = {.number = scalar(coordinates, distance.number)};
        output[24] = converted.bits;
        print_words(output, 25);
    }
    semihost(4, "AB coordinates reference complete\n");
}

#endif
