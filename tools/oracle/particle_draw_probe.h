#ifndef AB_ORACLE_PARTICLE_DRAW_H
#define AB_ORACLE_PARTICLE_DRAW_H

static uint32_t particle_draw_rows[2][29], particle_draw_count;

static uint32_t *particle_draw_state(uint32_t *context) {
    return (uint32_t *)(uintptr_t)context[1];
}

static void particle_draw_image(void *image, uint32_t *context, float x, float y,
                                float width, float height, const uint32_t *uv,
                                void *shader, void *color) {
    (void)image;
    if (particle_draw_count == 2 || shader || color)
        fail("Unexpected native particle image boundary\n");
    uint32_t *row = particle_draw_rows[particle_draw_count++];
    const uint32_t *state = particle_draw_state(context);
    size_t output = 1;
    for (size_t index = 4; index < 14; ++index)
        row[output++] = state[index];
    row[output++] = state[18];
    row[output++] = state[16];
    for (size_t index = 0; index < 4; ++index)
        row[output++] = state[index];
    FloatBits rectangle[] = {
        {.number = x}, {.number = y}, {.number = width}, {.number = height}};
    for (size_t index = 0; index < 4; ++index)
        row[output++] = rectangle[index].bits;
    for (size_t index = 0; index < 8; ++index)
        row[output++] = uv[index];
    if (output != 29)
        fail("Invalid native particle draw record width\n");
}

static void particle_draw_pass(uint32_t *particles, uint32_t scene, uint32_t phase,
                               uint32_t seconds, int layer) {
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
    uint32_t begin = particles[0x3c / 4], end = particles[0x40 / 4];
#elif defined(AB_ORACLE_PARTICLE_MENU_ONLY)
    uint32_t begin = particles[0x30 / 4], end = particles[0x34 / 4];
#else
    uint32_t begin = particles[0x24 / 4], end = particles[0x28 / 4];
#endif
    if (end < begin || (end - begin) % 76 || (end - begin) / 76 > 2)
        fail("Unexpected native particle draw vector size\n");
    uint32_t count = (end - begin) / 76, selected = 0;
    uint32_t *records = (uint32_t *)(uintptr_t)begin;
#if defined(AB_ORACLE_PARTICLE_ZERO_SCALE)
    /* Controlled ParticleData inputs; original draw instructions are unchanged. */
    for (uint32_t index = 0; index < count; ++index)
        records[index * 19 + 11] = scene & 1 ? UINT32_C(0x80000000) : 0;
#endif
    for (uint32_t index = 0; index < count; ++index)
        if (!layer || records[index * 19 + 6] == (uint32_t)layer)
            ++selected;
    uint32_t header[] = {scene, phase, seconds, selected};
    print_words(header, 4);
    uint32_t camera[30] = {0};
    const float camera_values[] = {12.25f, -30.5f, 0.75f, 1.25f};
    for (size_t index = 0; index < 4; ++index) {
        FloatBits value = {.number = camera_values[index]};
        camera[26 + index] = value.bits;
    }
    void (*set_camera)(void *, void *) = (void (*)(void *, void *))find_function(
        "_ZN9Particles9setCameraEPN3gfx6CameraE");
    void (*draw)(void *, void *, int) = (void (*)(void *, void *, int))find_function(
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
        "_ZN9Particles18drawLevelParticlesEPN2gr7ContextEi");
#else
        "_ZN9Particles4drawEPN2gr7ContextEb");
#endif
    uint32_t context_methods[25] = {0}, context[2] = {0}, state[39] = {0};
    context_methods[24] = (uint32_t)(uintptr_t)particle_draw_state;
    context[0] = (uint32_t)(uintptr_t)context_methods;
    context[1] = (uint32_t)(uintptr_t)state;
    /* Retain explicit inherited color/opacity. Particles must not replace it. */
    const uint32_t color[] = {0x3f400000, 0x3f000000, 0x3e800000, 0x3f800000};
    for (size_t index = 0; index < 4; ++index)
        state[index] = color[index];
    state[16] = UINT32_C(0x3f000000);
#if defined(AB_ORACLE_PARTICLE_MENU_ONLY)
    /* Prove that menu draw overwrites inherited translation and scale, rather
     * than inheriting these or consulting the deliberately nonidentity camera. */
    state[8] = UINT32_C(0x42580000);
    state[9] = UINT32_C(0xc2280000);
    state[10] = UINT32_C(0x40c00000);
    state[11] = UINT32_C(0x41100000);
#endif
    uint32_t image_methods[7] = {0}, image[1], sheet[4] = {0};
    image_methods[6] = (uint32_t)(uintptr_t)particle_draw_image;
    image[0] = (uint32_t)(uintptr_t)image_methods;
    sheet[3] = (uint32_t)(uintptr_t)image;
    particle_box_sprite[0] = (uint32_t)find_symbol("_ZTVN4game6SpriteE", 1) + 8;
    particle_box_sprite[2] = (uint32_t)(uintptr_t)sheet;
    particle_box_sprite[5] = 13 | (17 << 16);
    FloatBits pivot_x = {.number = 3}, pivot_y = {.number = -2};
    particle_box_sprite[6] = pivot_x.bits;
    particle_box_sprite[7] = pivot_y.bits;
    const uint32_t uv[] = {0x3e000000, 0x3e800000, 0x3ea80000, 0x3e800000,
                           0x3e000000, 0x3f040000, 0x3ea80000, 0x3f040000};
    for (size_t index = 0; index < 8; ++index)
        particle_box_sprite[8 + index] = uv[index];
    particle_draw_count = 0;
    set_camera(particles, camera);
    draw(particles, context,
#if defined(AB_ORACLE_PARTICLE_MENU_ONLY)
         1
#else
         layer
#endif
    );
    if (particle_draw_count != selected)
        fail("Native particle draw omitted an ordinary sprite\n");
    size_t output = 0;
    for (uint32_t index = 0; index < count; ++index) {
        if (layer && records[index * 19 + 6] != (uint32_t)layer)
            continue;
        particle_draw_rows[output][0] =
            particle_box_sprite_id((const char *)(uintptr_t)records[index * 19 + 1]);
        print_words(particle_draw_rows[output++], 29);
    }
}

static void particle_draw_snapshot(uint32_t *particles, uint32_t scene, uint32_t phase,
                                   uint32_t seconds) {
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
    for (int layer = 0; layer < 3; ++layer)
        particle_draw_pass(particles, scene, phase, seconds, layer);
#else
    particle_draw_pass(particles, scene, phase, seconds, 0);
#endif
}

#endif
