#ifndef AB_MENU_PIGS_PREVIEW_H
#define AB_MENU_PIGS_PREVIEW_H

#include "ab/particle_draw.h"

struct PreviewPigs {
    PreviewMenu *menu;
    AbUiLevelPigs controls;
    AbTargetData definition;
    AbParticleSource source;
    AbParticles *particles;
    bool loaded;
};

static bool menu_pigs_sprite(void *context, AbSpan name, uintptr_t *output,
                             AbError *error) {
    (void)error;
    PreviewPigs *pigs = context;
    const AbSpriteProvider *provider = &pigs->menu->sprites.provider;
    AbSpriteAtlasBinding sprite;
    *output = provider->sprite(provider->context, name, &sprite)
                  ? (uintptr_t)sprite.image
                  : 0;
    return true;
}

static bool menu_pigs_assets(void *context, bool loading, AbError *error) {
    PreviewPigs *pigs = context;
    PreviewMenu *menu = pigs->menu;
    AbDataEntry entry = {{.kind = AB_DATA_NUMBER, .bits = menu_number(1).number_bits},
                         menu_data("GREEN_UPSELL")};
    AbDataTable request = {&entry, 1, 1, NULL};
    if (!ab_android_assets_manage(menu->planning, &request, loading,
                                  ab_asset_resources_host, menu->resources, error))
        return false;
    pigs->loaded = loading;
    if (!ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error))
        return false;
    if (!loading) {
        /* Source exit releases GREEN_UPSELL before particles.clear("menu"). */
        ab_particles_close(pigs->particles);
        pigs->particles = NULL;
        ab_target_data_close(&pigs->definition);
        return true;
    }
    if (pigs->particles)
        return ab_fail(error, "Bad Piggies particles already entered");
    if (!ab_target_data_open(menu->archive,
                             "assets/data/scripts/particles/green_rocket.lua",
                             (AbSpan){menu->key, sizeof(menu->key)}, 256 * 1024, NULL,
                             &pigs->definition, error))
        return false;
    pigs->source =
        (AbParticleSource){menu_span("green_rocket"), pigs->definition.data.globals};
    return ab_particles_open((AbParticleConfig){.sources = &pigs->source,
                                                .source_count = 1,
                                                .capacity = 256,
                                                .storage_bytes = 256 * 1024,
                                                .random = &menu->engine_random,
                                                .admission = {75, 150, 0.5f},
                                                .context = pigs,
                                                .sprite = menu_pigs_sprite},
                             &pigs->particles, error);
}

static bool menu_pigs_emit(void *context, const AbParticleEmission *emission,
                           AbError *error) {
    return ab_particles_emit(((PreviewPigs *)context)->particles, emission, error);
}

static bool menu_pigs_draw(void *context, AbError *error) {
    PreviewPigs *pigs = context;
    PreviewMenu *menu = pigs->menu;
    /* GameLua::drawMenuParticlesInAdvance ELF VA0x3070b8 draws the alternate list
     * immediately and suppresses its usual end-of-update draw. Source global
     * g_menu_particle_scale starts at1 for this ordinary Android menu route. */
    return ab_particles_draw_menu(
        pigs->particles,
        (AbParticleDrawStyle){.provider = &menu->sprites.provider,
                              .width = (uint32_t)menu->screen.x,
                              .height = (uint32_t)menu->screen.y,
                              .blend = AB_BLEND_PREMULTIPLIED,
                              .color = {1, 1, 1, 1},
                              .opacity = 1},
        1, menu->context.draw_list, error);
}

static void menu_pigs_free(PreviewMenu *menu) {
    if (!menu->pigs)
        return;
    PreviewPigs *pigs = menu->pigs;
    if (pigs->loaded) {
        AbError ignored = {0};
        (void)menu_pigs_assets(pigs, false, &ignored);
    }
    ab_particles_close(pigs->particles);
    ab_target_data_close(&pigs->definition);
    free(pigs);
    menu->pigs = NULL;
}

static bool menu_pigs_attach(PreviewMenu *menu, AbError *error) {
    PreviewPigs *pigs = calloc(1, sizeof(*pigs));
    if (!pigs)
        return ab_fail(error, "Cannot allocate Bad Piggies level-screen owner");
    menu->pigs = pigs;
    pigs->menu = menu;
    return ab_ui_level_pigs_attach(&pigs->controls, menu->levels,
                                   (AbUiLevelPigsHost){.random = &menu->random,
                                                       .context = pigs,
                                                       .assets = menu_pigs_assets,
                                                       .emit = menu_pigs_emit,
                                                       .draw_particles = menu_pigs_draw,
                                                       .has_web_browser = true},
                                   error);
}

static bool menu_pigs_tail(PreviewMenu *menu, float seconds, AbError *error) {
    /* GameLua::update ELF VA0x323ec0 calls script update/draw before updating the menu
     * particle list. Preserve its captured draw state and then advance storage. */
    return !menu->pigs || !menu->pigs->particles ||
           ab_particles_update(menu->pigs->particles, seconds, true, error);
}

#endif
