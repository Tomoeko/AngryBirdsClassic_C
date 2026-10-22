#ifndef AB_TEST_CLASSIC_GAME_DRAW_NATIVE_H
#define AB_TEST_CLASSIC_GAME_DRAW_NATIVE_H

static FILE *lifecycle_draw_native;

static bool lifecycle_draw_words(uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (fscanf(lifecycle_draw_native, "%" SCNx32, words + index) != 1)
            return false;
    return true;
}

static bool lifecycle_draw_fullscreen(LifecycleGameDraw *draw, const uint32_t event[8],
                                      AbDrawList *list, AbError *error) {
    CHECK(event[1] == 4 || (event[1] == 5 && !event[6]));
    AbGameRectangle rectangle = {.right = 480, .bottom = 320};
    for (size_t channel = 0; channel < 4; ++channel)
        rectangle.color[channel] = from_bits(event[channel + 2]);
    AbGameRectanglePlan plan;
    CHECK(ab_game_rectangle_plan(&rectangle, &plan, error));
    draw->theme.render =
        (AbThemeLayerRenderState){.geometry = {.scale_x = 1, .scale_y = 1}, .alpha = 1};
    AbDrawCommand command = {0};
    bool visible = false;
    CHECK(ab_fill_rectangle_reset_command(
              (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width, plan.height,
                                AB_FILL_BLEND_AUTO},
              480, 320, draw->theme.white_texture, &command, &visible, error) &&
          (!visible || ab_draw_list_push(list, &command, error)));
    return true;
}

/* Feed original boundary order/arguments to independently tested real command
 * producers. No expected sequence is copied from classic_game_draw.c. */
static bool lifecycle_draw_native_boundary(LifecycleGameDraw *draw,
                                           const uint32_t event[8], AbDrawList *list,
                                           AbError *error) {
    AbClassicGameDrawConfig *config = &draw->owner.config;
    AbCoordinateCamera camera =
        ab_classic_camera_view(config->camera).system.viewport->rendering;
    AbParticleDrawStyle particles = {.provider = config->provider,
                                     .camera = camera,
                                     .width = 480,
                                     .height = 320,
                                     .blend = AB_BLEND_PREMULTIPLIED,
                                     .color = {1, 1, 1, 1},
                                     .opacity = draw->theme.render.alpha};
    switch (event[0]) {
        case 1: {
            float rgb[3];
            for (size_t channel = 0; channel < 3; ++channel)
                rgb[channel] = from_bits(event[channel + 2]);
            return ab_background_color_rgb(rgb, &draw->theme.background, error);
        }
        case 2:
            return lifecycle_draw_fullscreen(draw, event, list, error);
        case 3:
            draw->font = span("FONT_BASIC");
            return true;
        case 4:
        case 6:
        case 17:
            return true;
        case 5:
        case 14:
            return theme_preview_draw(&draw->theme, camera, event[0] == 14, list,
                                      error);
        case 7:
            return ab_classic_sling_draw_calculate(draw->sling, error);
        case 8:
        case 12:
        case 15: {
            AbSlingshotDrawPart part = event[0] == 8    ? AB_SLING_DRAW_BACK
                                       : event[0] == 12 ? AB_SLING_DRAW_HOLDER
                                                        : AB_SLING_DRAW_FRONT;
            return ab_classic_sling_draw_part(draw->sling, camera, 480, 320, part, list,
                                              error);
        }
        case 9:
            return ab_classic_game_powerups_draw(&draw->powerups, from_bits(event[3]),
                                                 list, error);
        case 10:
            return ab_classic_render_scene_sync(config->scene, error) &&
                   ab_classic_scene_sync_visuals(
                       ab_classic_render_scene_objects(config->scene), config->visuals,
                       error) &&
                   ab_classic_render_scene_draw(config->scene, camera, 480, 320, list,
                                                error);
        case 11:
            return ab_particles_draw_world(config->particles, particles, list, error);
        case 13: {
            float layer = from_bits(event[2]);
            CHECK(layer == 1 || layer == 2);
            return ab_particles_draw_level(config->particles, particles, (int32_t)layer,
                                           list, error);
        }
        case 16:
            draw->theme.render.geometry =
                (AbSpriteRenderState){.scale_x = 1, .scale_y = 1};
            return true;
        default:
            return ab_fail(error, "Unexpected retained original drawGame boundary");
    }
}

static bool lifecycle_draw_command_equal(const AbDrawCommand *actual,
                                         const AbDrawCommand *expected) {
    if (actual->texture != expected->texture || actual->blend != expected->blend ||
        actual->coordinates != expected->coordinates ||
        bits(actual->opacity) != bits(expected->opacity))
        return false;
    for (size_t channel = 0; channel < 4; ++channel)
        if (bits(actual->color[channel]) != bits(expected->color[channel]))
            return false;
    for (size_t index = 0; index < 4; ++index) {
        const AbDrawVertex *a = actual->quad.vertices + index;
        const AbDrawVertex *b = expected->quad.vertices + index;
        if (bits(a->x) != bits(b->x) || bits(a->y) != bits(b->y) ||
            bits(a->u) != bits(b->u) || bits(a->v) != bits(b->v))
            return false;
    }
    return true;
}

static bool lifecycle_game_draw_native(LifecycleGameDraw *draw, AbDrawList *actual,
                                       AbError *error) {
    if (!lifecycle_draw_native)
        return true;
    char header[80];
    CHECK(fgets(header, sizeof(header), lifecycle_draw_native) &&
          !strcmp(header, "AB_ANDROID_GAME_DRAW_V1 16\n"));
    AbClassicGameDrawState saved = draw->state;
    AbDataTable settings = draw->settings;
    AbDataEntry quality = {{.kind = AB_DATA_STRING, .string = span("gfxLowQuality")},
                           {.kind = AB_DATA_NUMBER}};
    AbDataEntry rgba[4] = {{{.kind = AB_DATA_STRING, .string = span("r")},
                            {.kind = AB_DATA_NUMBER, .bits = bits(0.1f)}},
                           {{.kind = AB_DATA_STRING, .string = span("g")},
                            {.kind = AB_DATA_NUMBER, .bits = bits(0.2f)}},
                           {{.kind = AB_DATA_STRING, .string = span("b")},
                            {.kind = AB_DATA_NUMBER, .bits = bits(0.3f)}},
                           {{.kind = AB_DATA_STRING, .string = span("a")},
                            {.kind = AB_DATA_NUMBER, .bits = bits(0.4f)}}};
    AbDataTable overlay = {rgba, 4, 4, NULL};
    size_t matched = 0, boundaries = 0;
    for (uint32_t sample = 0; sample < 16; ++sample) {
        uint32_t meta[2], events[32][8], result[6];
        CHECK(lifecycle_draw_words(meta, 2) && meta[0] == sample && meta[1] <= 32);
        for (uint32_t index = 0; index < meta[1]; ++index)
            CHECK(lifecycle_draw_words(events[index], 8));
        CHECK(lifecycle_draw_words(result, 6));
        draw->settings =
            sample & 1 ? (AbDataTable){&quality, 1, 1, NULL} : (AbDataTable){0};
        draw->state.background_overlay =
            sample & 2 ? (AbDataValue){.kind = AB_DATA_TABLE, .table = &overlay}
                       : (AbDataValue){0};
        draw->state.eagle_darkness =
            sample & 4 ? (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(0.25f)}
                       : (AbDataValue){0};
        draw->state.level_particles_enabled =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = (sample & 8) != 0};
        draw->state.zoom_level =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(1.25f)};
        draw->state.has_extra_z_ordered_draws =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        actual->count = 0;
        CHECK(ab_classic_game_draw(&draw->owner, 480, 320, 4, actual, error));
        CHECK(result[0] == 3 && result[1] == draw->state.old_zoom_level.bits &&
              draw->state.old_zoom_level.kind == AB_DATA_NUMBER && result[2] == 1 &&
              result[3] == draw->state.has_extra_z_ordered_draws.bits &&
              draw->state.has_extra_z_ordered_draws.kind == AB_DATA_BOOLEAN &&
              result[4] ==
                  (draw->state.eagle_darkness.kind == AB_DATA_NUMBER ? 3u : 0) &&
              result[5] == draw->state.eagle_darkness.bits);
        AbDrawCommand expected_commands[128];
        AbDrawList expected = {expected_commands, 0, 128};
        for (uint32_t index = 0; index < meta[1]; ++index)
            CHECK(
                lifecycle_draw_native_boundary(draw, events[index], &expected, error));
        CHECK(actual->count == expected.count);
        for (size_t index = 0; index < expected.count; ++index) {
            if (!lifecycle_draw_command_equal(actual->commands + index,
                                              expected.commands + index)) {
                fprintf(stderr, "Native drawGame order case %u command %zu\n", sample,
                        index);
                return false;
            }
        }
        matched += expected.count;
        boundaries += meta[1];
    }
    CHECK(fscanf(lifecycle_draw_native, " %79s", header) == 1 && !strcmp(header, "AB"));
    CHECK(fgets(header, sizeof(header), lifecycle_draw_native) &&
          !strcmp(header, " drawGame reference complete\n") &&
          fgetc(lifecycle_draw_native) == EOF);
    draw->state = saved;
    draw->settings = settings;
    printf("Owned drawGame matches all %zu commands composed from %zu original "
           "boundaries across 16 quality/overlay/darkness/layer cases. This does "
           "not compare complete native GPU frames.\n",
           matched, boundaries);
    return true;
}

#endif
