#ifndef AB_UI_SCORE_HUD_APK_TEST_H
#define AB_UI_SCORE_HUD_APK_TEST_H

typedef struct {
    bool zero, total, hit, pig, expired;
    const AbBirdCollisionFloatingScore *retained;
} ScoreHudCheck;

static bool score_test_uv(const AbDrawCommand *command, const AbUiFontBinding *font,
                          uint32_t code) {
    const AbFontGlyph *glyph = ab_font_glyph(font->font, code);
    if (!glyph || command->texture != font->texture)
        return false;
    return fabsf(command->quad.vertices[0].u -
                 (float)glyph->x / (float)font->texture_width) < 0.000001f &&
           fabsf(command->quad.vertices[0].v -
                 (float)glyph->y / (float)font->texture_height) < 0.000001f;
}

/* Check the current numeric value's actual glyph sequence in the top-right
 * gameplay region, including after the fixture's two desktop resizes. */
static bool score_test_total(PreviewGame *game, ScoreHudCheck *check, AbError *error) {
    PreviewMenu *menu = game->menu;
    const AbUiFontBinding *font = NULL;
    for (size_t index = 0; index < menu->fonts.count; ++index)
        if (menu->fonts.bindings[index].name.size == 10 &&
            !memcmp(menu->fonts.bindings[index].name.data, "FONT_BASIC", 10))
            font = menu->fonts.bindings + index;
    char text[32];
    int length =
        snprintf(text, sizeof(text), "%d", (int)menu_real(game->globals.score.bits));
    if (!font || length < 1 || (size_t)length >= sizeof(text))
        return ab_fail(error, "Score HUD test lacks its actual numeric font/value");
    AbDrawList *list = menu->context.draw_list;
    for (size_t start = 0; start + (size_t)length <= list->count; ++start) {
        const AbDrawCommand *command = list->commands + start;
        if (command->coordinates != AB_DRAW_CLIP || command->quad.vertices[0].x < 0 ||
            command->quad.vertices[0].y < 0.4f)
            continue;
        bool match = true;
        for (size_t index = 0; index < (size_t)length; ++index)
            match = match && score_test_uv(command + index, font, (uint8_t)text[index]);
        if (match) {
            for (size_t index = 0; index < (size_t)length; ++index) {
                const AbDrawCommand *glyph = command + index;
                float right = (glyph->quad.vertices[3].x + 1) * game->screen.x / 2;
                float top = (1 - glyph->quad.vertices[0].y) * game->screen.y / 2;
                if (glyph->opacity != 1 || glyph->color[3] != 1 ||
                    (glyph->scissor.enabled && ((float)glyph->scissor.right < right ||
                                                (float)glyph->scissor.top > top)))
                    return ab_fail(error, "Gameplay score inherits a stale clip/alpha");
            }
            check->zero |= length == 1 && text[0] == '0';
            check->total |= menu_real(game->globals.score.bits) > 0;
            return true;
        }
    }
    return ab_fail(error, "Actual gameplay draw omitted top-right score %s", text);
}

static bool score_test_capture(PreviewMenu *menu, AbMetal *renderer, const char *name,
                               AbError *error) {
    const char *path = getenv(name);
    if (!path)
        return true;
    AbImage image = {0};
    const float clear[4] = {0, 0, 0, 1};
    bool okay =
        ab_metal_capture(renderer, menu->context.draw_list,
                         (uint32_t)menu->game->screen.x, (uint32_t)menu->game->screen.y,
                         clear, 16 * 1024 * 1024, &image, error) &&
        save(path, &image, error);
    ab_image_free(&image);
    return okay;
}

/* Use the live collision/removal records, atlas and viewport, then require that
 * those commands actually reached the application's complete world/HUD frame.
 * A disconnected renderer therefore fails even when its isolated API works. */
static bool score_test_frame(PreviewGame *game, AbMetal *renderer, ScoreHudCheck *check,
                             AbError *error) {
    if (!score_test_total(game, check, error))
        return false;
    PreviewMenu *menu = game->menu;
    AbFloatingScores *scores = ab_classic_world_runtime_view(game->world).floating;
    if (check->retained && check->retained->time > check->retained->lifetime)
        check->expired = true;
    bool hit = false, pig = false;
    for (size_t index = 0; index < ab_floating_scores_count(scores); ++index) {
        const AbBirdCollisionFloatingScore *score =
            ab_floating_scores_at(scores, index);
        if (score->xs < score->max_scale * 0.9f)
            continue;
        hit |= score->text_present;
        pig |= score->score == 5000 && score->sprite.kind == AB_DATA_STRING;
        if (!check->retained)
            check->retained = score;
    }
    if ((!hit || check->hit) && (!pig || check->pig))
        return true;
    AbDrawCommand commands[512];
    AbDrawList expected = {commands, 0, 512};
    AbCameraViewport viewport;
    float scale;
    if (!ab_camera_system_query_viewport(
            ab_classic_camera_system(game->playfield.camera), &viewport) ||
        !ab_ui_scaling_get(&menu->scaling, &scale, error) ||
        !ab_floating_scores_draw(scores,
                                 &(AbFloatingScoreDrawResources){
                                     .font = &game->score_font.font,
                                     .texture = 70,
                                     .texture_width = game->score_font.image.width,
                                     .texture_height = game->score_font.image.height,
                                     .strings = &menu->text_resources,
                                     .sprites = &menu->sprites.provider,
                                     .codes = menu->codes,
                                     .glyphs = menu->glyphs,
                                     .capacity = 128},
                                 viewport.rendering, scale, (uint32_t)game->screen.x,
                                 (uint32_t)game->screen.y, &expected, error))
        return false;
    if (!expected.count)
        return true;
    const AbDrawList *actual = menu->context.draw_list;
    bool found = false;
    for (size_t start = 0; start + expected.count <= actual->count && !found; ++start) {
        found = true;
        for (size_t index = 0; index < expected.count; ++index) {
            const AbDrawCommand *a = actual->commands + start + index;
            const AbDrawCommand *b = expected.commands + index;
            found = found && a->texture == b->texture && a->blend == b->blend &&
                    a->opacity == b->opacity &&
                    !memcmp(&a->quad, &b->quad, sizeof(a->quad));
        }
    }
    if (!found)
        return ab_fail(error, "Live collision score popups omitted from game draw");
    if (hit && !check->hit &&
        !score_test_capture(menu, renderer, "AB_SCORE_HIT_CAPTURE", error))
        return false;
    if (pig && !check->pig &&
        !score_test_capture(menu, renderer, "AB_SCORE_PIG_CAPTURE", error))
        return false;
    check->hit |= hit;
    check->pig |= pig;
    return true;
}

#endif
