#ifndef AB_TEST_PARTICLE_DRAW_H
#define AB_TEST_PARTICLE_DRAW_H

#include "ab/particle_draw.h"
#include "ab/rotation.h"

typedef struct {
    AbSpriteRegion region;
    AbImage image;
} ParticleDrawSource;

static bool draw_sprite(void *context, AbSpan name, AbSpriteAtlasBinding *output) {
    ParticleDrawSource *source = context;
    (void)name;
    *output = (AbSpriteAtlasBinding){.sprite = {.region = &source->region},
                                     .image = &source->image};
    return true;
}

static bool draw_texture(void *context, const AbImage *image, uint32_t *output) {
    ParticleDrawSource *source = context;
    if (image != &source->image)
        return false;
    *output = 7;
    return true;
}

static bool compare_draw_snapshot(AbParticles *owner, FILE *file, uint32_t scene,
                                  uint32_t phase, uint32_t seconds, bool level,
                                  bool menu, int32_t layer) {
    size_t count =
        level ? ab_particles_level_count(owner) : ab_particles_count(owner, menu);
    size_t selected = 0;
    for (size_t index = 0; index < count; ++index) {
        const AbParticle *particle = level ? ab_particles_level_at(owner, index)
                                           : ab_particles_at(owner, menu, index);
        CHECK(particle);
        selected += !layer || particle->layer == layer;
    }
    uint32_t header[4];
    CHECK(read_words(file, header, 4) && header[0] == scene && header[1] == phase &&
          header[2] == seconds && header[3] == selected);
    ParticleDrawSource source = {
        .region =
            {.x = 8, .y = 16, .width = 13, .height = 17, .pivot_x = 3, .pivot_y = -2},
        .image = {.width = 64, .height = 64}};
    AbSpriteProvider provider = {.context = &source,
                                 .sprite = draw_sprite,
                                 .composite = NULL,
                                 .texture = draw_texture};
    AbParticleDrawStyle style = {.provider = &provider,
                                 .camera = {{12.25f, -30.5f}, {0.75f, 1.25f}},
                                 .width = 480,
                                 .height = 320,
                                 .blend = AB_BLEND_PREMULTIPLIED,
                                 .color = {0.75f, 0.5f, 0.25f, 1},
                                 .opacity = 0.5f};
    const float menu_scales[] = {1.375f, 0.625f, -1.25f};
    float menu_scale = menu_scales[scene];
    if (menu)
        style.camera = (AbCoordinateCamera){{NAN, NAN}, {NAN, NAN}};
    size_t visible_count = 0;
    for (size_t index = 0; index < count; ++index) {
        const AbParticle *particle = level ? ab_particles_level_at(owner, index)
                                           : ab_particles_at(owner, menu, index);
        if (layer && particle->layer != layer)
            continue;
        AbImageRectangle rectangle;
        AbSpriteRenderState state;
        AbRotation rotation;
        CHECK(
            (menu ? ab_menu_particle_sprite_state(particle, &source.region, menu_scale,
                                                  &rectangle, &state, NULL)
             : level ? ab_level_particle_sprite_state(particle, &source.region,
                                                      style.camera, layer, &rectangle,
                                                      &state, NULL)
                     : ab_particle_sprite_state(particle, &source.region, style.camera,
                                                &rectangle, &state, NULL)) &&
            ab_rotation_from_angle(state.angle_radians, &rotation));
        uint32_t expected[29], actual[] = {sprite_id(particle->sprite),
                                           bits(rotation.cosine),
                                           bits(rotation.sine),
                                           bits(-rotation.sine),
                                           bits(rotation.cosine),
                                           bits(state.translation_x),
                                           bits(state.translation_y),
                                           bits(state.scale_x),
                                           bits(state.scale_y),
                                           bits(state.rotation_origin_x),
                                           bits(state.rotation_origin_y),
                                           bits(state.angle_radians),
                                           bits(style.opacity),
                                           bits(style.color[0]),
                                           bits(style.color[1]),
                                           bits(style.color[2]),
                                           bits(style.color[3]),
                                           bits(rectangle.x),
                                           bits(rectangle.y),
                                           bits(rectangle.width),
                                           bits(rectangle.height),
                                           UINT32_C(0x3e000000),
                                           UINT32_C(0x3e800000),
                                           UINT32_C(0x3ea80000),
                                           UINT32_C(0x3e800000),
                                           UINT32_C(0x3e000000),
                                           UINT32_C(0x3f040000),
                                           UINT32_C(0x3ea80000),
                                           UINT32_C(0x3f040000)};
        CHECK(read_words(file, expected, 29));
        for (size_t word = 0; word < 29; ++word) {
            if (actual[word] != expected[word]) {
                fprintf(stderr,
                        "Particle draw %u/%u/%zu word %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        scene, phase, index, word, actual[word], expected[word]);
                return false;
            }
        }
        float uv[8];
        for (size_t word = 0; word < 8; ++word)
            memcpy(uv + word, expected + 21 + word, sizeof(float));
        AbDrawQuad quad;
        bool visible;
        CHECK(ab_image_rectangle_quad(rectangle, state, style.width, style.height, uv,
                                      &quad, &visible, NULL));
        visible_count += visible;
    }
    AbDrawCommand commands[4];
    AbDrawList list = {commands, 0, 4};
    CHECK((menu    ? ab_particles_draw_menu(owner, style, menu_scale, &list, NULL)
           : level ? ab_particles_draw_level(owner, style, layer, &list, NULL)
                   : ab_particles_draw_world(owner, style, &list, NULL)) &&
          list.count == visible_count);
    for (size_t index = 0; index < list.count; ++index)
        CHECK(list.commands[index].texture == 7 &&
              list.commands[index].coordinates == AB_DRAW_CLIP &&
              !memcmp(list.commands[index].color, style.color, sizeof(style.color)) &&
              list.commands[index].opacity == style.opacity);
    /* A failed append preserves a pre-existing list prefix. */
    if (visible_count) {
        list.count = list.capacity;
        CHECK(!(menu    ? ab_particles_draw_menu(owner, style, menu_scale, &list, NULL)
                : level ? ab_particles_draw_level(owner, style, layer, &list, NULL)
                        : ab_particles_draw_world(owner, style, &list, NULL)) &&
              list.count == 4);
    }
    return true;
}

#endif
