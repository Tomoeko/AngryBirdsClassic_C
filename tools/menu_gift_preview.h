#ifndef AB_MENU_GIFT_PREVIEW_H
#define AB_MENU_GIFT_PREVIEW_H

#include "ab/particle_draw.h"

static bool menu_gift_sprite(void *context, AbSpan name, uintptr_t *output,
                             AbError *error) {
    (void)error;
    PreviewMenu *menu = context;
    AbSpriteAtlasBinding binding;
    *output =
        menu->sprites.provider.sprite(menu->sprites.provider.context, name, &binding)
            ? (uintptr_t)binding.image
            : 0;
    return true;
}

static bool menu_gift_open(PreviewMenu *menu, AbError *error) {
    if (menu->gift_particles)
        return true;
    if (!ab_target_data_open(menu->archive,
                             "assets/data/scripts/particles/giftBoxUnwrapBuff.lua",
                             (AbSpan){menu->key, sizeof(menu->key)}, 256 * 1024, NULL,
                             &menu->gift_definition, error))
        return false;
    menu->gift_source = (AbParticleSource){menu_span("giftBoxUnwrapBuff"),
                                           menu->gift_definition.data.globals};
    return ab_particles_open((AbParticleConfig){.sources = &menu->gift_source,
                                                .source_count = 1,
                                                .capacity = 256,
                                                .storage_bytes = 256 * 1024,
                                                .random = &menu->engine_random,
                                                .admission = {75, 150, 0.5f},
                                                .context = menu,
                                                .sprite = menu_gift_sprite},
                             &menu->gift_particles, error);
}

static bool menu_gift_tail(PreviewMenu *menu, float seconds, AbError *error) {
    if (!menu->gift_particles)
        return true;
    /* Native menu particles draw before their update. Preserve that order and
     * keep their lifetime advancing while another screen owns the main draw. */
    if (menu->episode_active &&
        !ab_particles_draw_menu(
            menu->gift_particles,
            (AbParticleDrawStyle){.provider = &menu->sprites.provider,
                                  .width = (uint32_t)menu->screen.x,
                                  .height = (uint32_t)menu->screen.y,
                                  .blend = AB_BLEND_PREMULTIPLIED,
                                  .color = {1, 1, 1, 1},
                                  .opacity = 1},
            1, menu->context.draw_list, error))
        return false;
    return ab_particles_update(menu->gift_particles, seconds, true, error);
}

#endif
