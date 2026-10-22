#ifndef AB_UI_MENU_BIRDS_APK_TEST_H
#define AB_UI_MENU_BIRDS_APK_TEST_H

static bool menu_bird_curves(PreviewMenu *menu, AbError *error) {
    AbUiMenuBirds controlled = {0};
    AbRandom48 random;
    if (!ab_random48_seed(&random, 123) ||
        !ab_ui_menu_birds_init(&controlled, &random, error))
        return false;
    controlled.actor_count = 2;
    controlled.actors[0] = (AbUiMenuBirdActor){.sprite = menu_span("BIRD_RED"),
                                               .position = {10, 20},
                                               .velocity = {2, -3},
                                               .angle_speed = 2,
                                               .scale = 1,
                                               .sprite_height = 20,
                                               .layer = 3};
    controlled.actors[1] = (AbUiMenuBirdActor){.sprite = menu_span("BIRD_RED"),
                                               .position = {10, 20},
                                               .velocity = {0, -4},
                                               .scale = 1,
                                               .sprite_height = 20,
                                               .layer = 4,
                                               .reward = 2};
    uint64_t retained = random.value;
    if (!ab_ui_menu_birds_update(&controlled, &menu->context, (AbVec2){480, 320}, 0.5f,
                                 error))
        return false;
    const AbUiMenuBirdActor *ordinary = controlled.actors;
    const AbUiMenuBirdActor *balloon = controlled.actors + 1;
    if (random.value != retained || controlled.actor_count != 2 ||
        ordinary->position.x != 11 || ordinary->position.y != 56 ||
        ordinary->velocity.y != 72 || ordinary->angle != 1 ||
        balloon->position.x != 10 || balloon->position.y != 18 || balloon->angle != 0 ||
        balloon->angle_speed != 1)
        return ab_fail(error, "Menu bird gravity/balloon ordering or RNG gate differs");
    controlled.actors[0].position.y = 400;
    controlled.actors[1].position.y = 401;
    if (!ab_ui_menu_birds_update(&controlled, &menu->context, (AbVec2){480, 320}, 0,
                                 error) ||
        controlled.actor_count != 1 || controlled.actors[0].position.y != 400)
        return ab_fail(error, "Menu bird strict lower-screen removal differs");
    controlled.actors[0].reward = 2;
    controlled.actors[0].position.y = -20;
    if (!ab_ui_menu_birds_update(&controlled, &menu->context, (AbVec2){480, 320}, 0,
                                 error) ||
        controlled.actor_count != 1)
        return ab_fail(error, "Menu balloon removed at its inclusive top boundary");
    controlled.actors[0].position.y = -21;
    return ab_ui_menu_birds_update(&controlled, &menu->context, (AbVec2){480, 320}, 0,
                                   error) &&
           !controlled.actor_count;
}

static bool menu_bird_settings(PreviewMenu *menu, AbError *error) {
    AbUiMenuBirds controlled = {0};
    AbSettingsStatistics *settings = NULL;
    AbRandom48 random;
    bool okay = false;
    if (!ab_random48_seed(&random, 456) ||
        !ab_ui_menu_birds_init(&controlled, &random, error) ||
        !ab_settings_statistics_open((AbSettingsStatisticsConfig){menu_span("fixture"),
                                                                  menu_span("fixture"),
                                                                  8, 8, 65536},
                                     &settings, error))
        goto release;
    AbSpan path[] = {menu_span("tutorials"), menu_span("BIRD_PINK")};
    if (!ab_settings_statistics_write(settings, path, 2,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN}, error))
        goto release;
    path[1] = menu_span("PORTALBLUE_1");
    if (!ab_settings_statistics_write(settings, path, 2,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                      error))
        goto release;
    path[1] = menu_span("BAIT_SARDINE");
    if (!ab_settings_statistics_write(settings, path, 2,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                      error) ||
        !ab_ui_menu_birds_build(&controlled, settings, &menu->statistics,
                                ab_episode_catalog_feather_ids(menu->catalog),
                                (AbVec2){480, 320}, menu_span("android"), false, error))
        goto release;
    if (controlled.candidate_count != 1 || controlled.candidates[0].sprite.size != 9 ||
        memcmp(controlled.candidates[0].sprite.data, "BIRD_PINK", 9))
        goto release;
    AbSpan unlocked = menu_span("mightyEagleUnlocked");
    if (!ab_settings_statistics_write(settings, &unlocked, 1,
                                      (AbDataValue){.kind = AB_DATA_NUMBER}, error) ||
        !ab_ui_menu_birds_build(&controlled, settings, &menu->statistics,
                                ab_episode_catalog_feather_ids(menu->catalog),
                                (AbVec2){480, 320}, menu_span("android"), false,
                                error) ||
        controlled.candidate_count != 2)
        goto release;
    AbSpan stars = menu_span("threeStarsLP1");
    controlled.actor_count = 1;
    if (!ab_settings_statistics_write(settings, &stars, 1,
                                      (AbDataValue){.kind = AB_DATA_NUMBER}, error) ||
        !ab_ui_menu_birds_build(&controlled, settings, &menu->statistics,
                                ab_episode_catalog_feather_ids(menu->catalog),
                                (AbVec2){960, 640}, menu_span("iphone"), false,
                                error) ||
        controlled.candidate_count != 4 || controlled.actor_count != 1 ||
        controlled.candidates[2].scale != 0.5f ||
        controlled.candidates[3].scale != 0.5f)
        goto release;
    okay = true;
release:
    ab_settings_statistics_close(settings);
    if (!okay && !error->message[0])
        ab_fail(error, "Menu bird tutorial/reward settings integration differs");
    return okay;
}

static bool menu_bird_widgets(PreviewMenu *menu, AbMetal *renderer, const char *path,
                              AbError *error) {
    if (!menu_bird_curves(menu, error) || !menu_bird_settings(menu, error) ||
        !menu_episode_change(menu, false, error))
        return false;
    AbSpriteAtlasBinding ordinary;
    AbResolvedComposite composite;
    if (!menu->sprites.provider.sprite(menu->sprites.provider.context,
                                       menu_span("BIRD_RED"), &ordinary) &&
        (!menu->sprites.provider.composite ||
         !menu->sprites.provider.composite(menu->sprites.provider.context,
                                           menu_span("BIRD_RED"), &composite)))
        return ab_fail(error, "Actual main-menu bird art is not bound");
    if (menu->birds.candidate_count != 1 ||
        menu->birds.candidates[0].sprite.size != 8 ||
        memcmp(menu->birds.candidates[0].sprite.data, "BIRD_RED", 8))
        return ab_fail(error, "Fresh preview did not use the source red-bird fallback");
    bool visible = false;
    AbDrawCommand bird_commands[AB_UI_MENU_BIRD_CAPACITY];
    for (size_t frame = 0; frame < 1000 && !visible; ++frame) {
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
        /* Observe the real bird quads without consuming RNG or changing the
         * application's render context. Require a hop above the foreground. */
        AbDrawList bird_list = {bird_commands, 0, AB_UI_MENU_BIRD_CAPACITY};
        AbUiImageRenderState state = menu->render;
        AbUiWidgetContext probe = menu->context;
        probe.render_state = &state;
        probe.draw_list = &bird_list;
        if (!ab_ui_menu_birds_draw(&menu->birds, &probe, &menu->sprites.provider,
                                   (AbVec2){480, 320}, error))
            return false;
        for (size_t index = 0; index < bird_list.count; ++index) {
            const AbDrawQuad *quad = &bird_list.commands[index].quad;
            float top = quad->vertices[0].y;
            for (size_t vertex = 1; vertex < 4; ++vertex)
                top = fmaxf(top, quad->vertices[vertex].y);
            visible = visible || top > -0.5f;
        }
    }
    if (!visible)
        return ab_fail(error, "No visible bird hop from the shared RNG fixture");
    AbImage capture = {0};
    float clear[4];
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list, 480, 320, clear,
                                 1024 * 1024, &capture, error) &&
                (!path || save(path, &capture, error));
    ab_image_free(&capture);
    if (okay)
        puts("Metal main menu: source bird fallback, settings reward gates, layered "
             "shared-RNG animation and theme1; native whole-screen fidelity remains "
             "open.");
    return okay;
}

#endif
