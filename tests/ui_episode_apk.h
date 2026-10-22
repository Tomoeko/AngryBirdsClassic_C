#ifndef AB_UI_EPISODE_APK_TEST_H
#define AB_UI_EPISODE_APK_TEST_H

#include "ab/ui_episode_button.h"
#include "ab/episode_catalog.h"
#include "ab/episode_statistics.h"
#include "ab/ui_episode_services.h"

#include <math.h>

#include "ui_episode_scroll_apk.h"

/* Controlled statistics/settings inputs for the real class, not application
 * defaults. Metadata, fonts, images, resource loading and rendering are real. */
typedef struct {
    AbEpisodeStatistics statistics;
    bool eagle, complete, seen, new_levels, visited;
    char calls[256];
    size_t count;
} EpisodeInputs;

static bool episode_query(EpisodeInputs *inputs, AbUiPointerValue episode, char call,
                          AbError *error) {
    if (episode.kind != AB_UI_POINTER_NUMBER ||
        episode.number_bits != UINT32_C(0x3f800000) ||
        inputs->count >= sizeof(inputs->calls) - 1)
        return ab_fail(error, "Wrong episode or excess episode-card query calls");
    inputs->calls[inputs->count++] = call;
    inputs->calls[inputs->count] = 0;
    return true;
}

static bool episode_stars(void *context, AbUiPointerValue episode, float *earned,
                          float *available, AbError *error) {
    if (!episode_query(context, episode, 'S', error))
        return false;
    EpisodeInputs *inputs = context;
    return ab_episode_statistics_stars(
        &inputs->statistics,
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = episode.number_bits}, false,
        earned, available, error);
}

static bool episode_score(void *context, AbUiPointerValue episode, float *output,
                          AbError *error) {
    if (!episode_query(context, episode, 'Q', error))
        return false;
    EpisodeInputs *inputs = context;
    return ab_episode_statistics_score(
        &inputs->statistics,
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = episode.number_bits}, output,
        error);
}

static bool episode_feathers(void *context, AbUiPointerValue episode, float *earned,
                             float *available, AbError *error) {
    if (!episode_query(context, episode, 'F', error))
        return false;
    EpisodeInputs *inputs = context;
    return ab_episode_statistics_feathers(
        &inputs->statistics,
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = episode.number_bits}, earned,
        available, error);
}

static bool episode_eagle(void *context, AbUiPointerValue episode, bool *output,
                          AbError *error) {
    EpisodeInputs *inputs = context;
    if (!episode_query(inputs, episode, 'E', error))
        return false;
    *output = inputs->eagle;
    return true;
}

static bool episode_complete(void *context, AbUiPointerValue episode, bool *output,
                             AbError *error) {
    EpisodeInputs *inputs = context;
    if (!episode_query(inputs, episode, 'C', error))
        return false;
    *output = inputs->complete;
    return true;
}

static bool episode_seen(void *context, AbUiPointerValue episode, bool *output,
                         AbError *error) {
    EpisodeInputs *inputs = context;
    if (!episode_query(inputs, episode, 'V', error))
        return false;
    *output = inputs->seen;
    return true;
}

static bool episode_new_levels(void *context, AbUiPointerValue episode, bool *output,
                               AbError *error) {
    EpisodeInputs *inputs = context;
    if (!episode_query(inputs, episode, 'N', error))
        return false;
    *output = inputs->new_levels;
    return true;
}

static bool episode_page(void *context, AbUiPointerValue episode,
                         AbUiPointerValue *output, AbError *error) {
    if (!episode_query(context, episode, 'P', error))
        return false;
    *output = menu_number(3);
    return true;
}

static bool episode_visited(void *context, AbUiPointerValue episode,
                            AbUiPointerValue page, bool *output, AbError *error) {
    EpisodeInputs *inputs = context;
    if (page.kind != AB_UI_POINTER_NUMBER || page.number_bits != UINT32_C(0x40400000) ||
        !episode_query(inputs, episode, 'T', error))
        return false;
    *output = inputs->visited;
    return true;
}

static bool episode_action(PreviewMenu *menu, AbUiEpisodeButton *button,
                           const char *expected, AbError *error) {
    AbUiPointerReply reply;
    const AbVec2 point = {240, 140};
    menu->context.touch_count = 1;
    if (!ab_ui_widget_pointer(&button->widget, menu_span("LPRESS"), point, &reply,
                              error))
        return false;
    menu->context.touch_count = 0;
    if (!ab_ui_widget_pointer(&button->widget, menu_span("LRELEASE"), point, &reply,
                              error))
        return false;
    return menu_equal(reply.values[0], expected) &&
                   reply.values[2].kind == AB_UI_POINTER_OBJECT &&
                   reply.values[2].object == (uintptr_t)&button->widget
               ? true
               : ab_fail(error, "EpisodeButton pointer reply differs: %s", expected);
}

static bool episode_services_widgets(PreviewMenu *menu, AbUiEpisodeButton *button,
                                     const AbDataTable *ids,
                                     AbEpisodeStatistics *statistics, AbMetal *renderer,
                                     const float clear[4], const char *first_path,
                                     const char *last_path, AbError *error) {
    /* Explicit test state, without a claim about purchase/SDK replies or initial
     * save defaults. Actual card queries read these live tables directly. */
    AbDataTable empty = {0};
    AbDataEntry feature_entry = {
        .key = {.kind = AB_DATA_STRING, .string = menu_span("g_freeEagleUnlockStatus")},
        .value = {.kind = AB_DATA_TABLE, .table = &empty}};
    AbDataTable globals = {.entries = &feature_entry, .count = 1, .capacity = 1};
    AbEpisodeProgress progress = {.episodes = statistics->episodes,
                                  .local_settings = &empty,
                                  .globals = &globals,
                                  .purchases = &empty};
    AbUiEpisodeServices services = {statistics, &progress};
    AbUiEpisodeButtonHost host = ab_ui_episode_services_host(&services);
    AbUiEpisodeUgcClass ugc = {0};
    AbDataEntry gift_entry = {
        .key = {.kind = AB_DATA_STRING,
                .string = menu_span("UGCEpisodeCardGiftBoxOpened")},
        .value = {.kind = AB_DATA_BOOLEAN}};
    AbDataTable settings = {.entries = &gift_entry, .count = 1, .capacity = 1};
    host.ugc_class = &ugc;
    host.settings = &settings;
    host.fonts = button->host.fonts;
    host.scaling = button->host.scaling;
    host.random = button->host.random;
    size_t count = 0;
    for (size_t index = 1; index <= ids->count; ++index) {
        AbUiEpisodeButton actual = {0};
        const AbDataValue *id = ab_data_get(
            ids, (AbDataValue){.kind = AB_DATA_NUMBER,
                               .bits = menu_number((float)index).number_bits});
        const AbDataValue *metadata =
            id ? ab_data_get(statistics->episodes, *id) : NULL;
        if (!metadata || metadata->kind != AB_DATA_TABLE) {
            ab_fail(error, "Missing actual episode-card service record");
            return false;
        }
        const AbDataValue *subclass = ab_data_get(
            metadata->table,
            (AbDataValue){.kind = AB_DATA_STRING,
                          .string = menu_span("episode_selection_button_subclass")});
        AbUiEpisodeButtonClass class_kind = AB_UI_EPISODE_STANDARD;
        if (subclass && subclass->kind != AB_DATA_NIL) {
            AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT,
                                     .text = subclass->string};
            if (menu_equal(name, "EggDefenderEpisodeButton"))
                class_kind = AB_UI_EPISODE_EGG_DEFENDER;
            else if (menu_equal(name, "GoldenEggButton"))
                class_kind = AB_UI_EPISODE_GOLDEN_EGGS;
            else if (menu_equal(name, "EpisodeButtonUGC"))
                class_kind = AB_UI_EPISODE_UGC;
            else
                return ab_fail(error, "Unknown actual episode-card subclass");
        }
        AbUiPointerValue episode =
            id->kind == AB_DATA_NUMBER
                ? (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                     .number_bits = id->bits}
                : (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = id->string};
        if (!ab_ui_episode_button_init_class(&actual, &menu->context, NULL, host,
                                             class_kind, error) ||
            !ab_ui_episode_button_set_episode(&actual, episode, metadata->table,
                                              error) ||
            !ab_ui_widget_lifecycle(&actual.widget, AB_UI_FRAME_ENTRY, error) ||
            !ab_ui_widget_lifecycle(&actual.widget, AB_UI_FRAME_LAYOUT, error))
            return false;
        actual.widget.frame->fields.x = menu_number(240);
        actual.widget.frame->fields.y = menu_number(140);
        if (actual.widget.frame->child_count !=
            (class_kind == AB_UI_EPISODE_GOLDEN_EGGS ? 3u
             : class_kind == AB_UI_EPISODE_UGC       ? 9u
                                                     : 8u))
            return ab_fail(error, "Wrong actual episode subclass child count");
        if (class_kind == AB_UI_EPISODE_UGC &&
            actual.score_box.frame->fields.x.number_bits != 0)
            return ab_fail(error, "UGC score-box horizontal clamp differs");
        ab_draw_list_clear(menu->context.draw_list);
        AbUiDrawInput phase = {.transform = {.scale = {1, 1}, .local_factor = 1},
                               .present = 63};
        const char *const names[] = {"drawBackground", "drawMidground",
                                     "drawForeground"};
        for (size_t pass = 0; pass < 3; ++pass)
            if (!ab_ui_widget_draw_named(&actual.widget, menu_span(names[pass]), &phase,
                                         error))
                return false;
        if (!menu->context.draw_list->count) {
            ab_fail(error, "Actual episode card emitted no draw commands");
            return false;
        }
        if (!ab_ui_widget_update(&actual.widget, 0.1f, 0.1f, error))
            return false;
        if (class_kind == AB_UI_EPISODE_UGC) {
            if (ugc.normal ||
                ab_ui_pointer_value_truthy(&actual.ribbon.frame->fields.visible))
                return ab_fail(error, "UGC gift box incorrectly starts unwrapped");
            gift_entry.value.bits = 1;
            if (!ab_ui_widget_update(&actual.widget, 0.2f, 0.3f, error) ||
                !ugc.normal ||
                !ab_ui_pointer_value_truthy(&actual.ribbon.frame->fields.visible) ||
                !ab_ui_widget_update(&actual.widget, 0.1f, 0.4f, error) ||
                actual.ribbon.frame->fields.scale_x.number_bits == UINT32_C(0x3f800000))
                return ab_fail(error, "UGC gift unwrap/ribbon tween differs");
        }
        if (!ab_ui_widget_lifecycle(&actual.widget, AB_UI_FRAME_EXIT, error))
            return false;
        ++count;
    }
    bool okay = count == 14 || ab_fail(error, "Wrong episode-card count: %zu", count);
    if (okay)
        puts("All 14 APK episode cards, including Golden Eggs, Egg Defender and UGC, "
             "use C "
             "services.");
    return okay && episode_scroll_widgets(menu, host, &progress, ids, renderer, clear,
                                          first_path, last_path, error);
}

static bool episode_widgets(const AbZip *zip, PreviewMenu *menu,
                            AbUiFontResources *fonts, AbMetal *renderer,
                            const float clear[4], const char *path,
                            const char *first_path, const char *last_path,
                            AbError *error) {
    AbEpisodeCatalog *catalog = NULL;
    AbTargetData stars = {0};
    AbUiEpisodeButton button = {0};
    EpisodeInputs inputs = {.seen = true};
    AbImage captured = {0};
    bool okay = false, entered = false;
    const AbDataTable *requirements = ab_dynamic_asset_requirements_get(
        &menu->requirements, menu_span("LevelSelectionRoot"));
    if (!requirements ||
        !ab_android_assets_manage(menu->planning, requirements, true,
                                  ab_asset_resources_host, menu->resources, error) ||
        !ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error) ||
        ab_asset_textures_count(menu->textures) >= 61 ||
        !ab_episode_catalog_open(
            zip, (AbSpan){menu->key, sizeof(menu->key)},
            (AbEpisodeCatalogConfig){.device_model = menu_span("android")},
            2 * 1024 * 1024, &catalog, error))
        goto release;
    const AbDataTable *episodes = ab_episode_catalog_episodes(catalog);
    AbDataEntry score_fields[] = {
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("score")},
         .value = {.kind = AB_DATA_NUMBER, .bits = menu_number(31480).number_bits}},
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("completed")},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("eagleScore")},
         .value = {.kind = AB_DATA_NUMBER, .bits = menu_number(100).number_bits}}};
    AbDataTable first_score = {.entries = score_fields, .count = 3, .capacity = 3};
    AbDataEntry score_entry = {
        .key = {.kind = AB_DATA_STRING, .string = menu_span("Level1")},
        .value = {.kind = AB_DATA_TABLE, .table = &first_score}};
    AbDataTable highscores = {.entries = &score_entry, .count = 1, .capacity = 1};
    if (!ab_target_data_open(zip, "assets/data/scripts/starLimits.lua",
                             (AbSpan){menu->key, sizeof(menu->key)}, 3 * 1024 * 1024,
                             NULL, &stars, error))
        goto release;
    inputs.statistics =
        (AbEpisodeStatistics){.episodes = episodes,
                              .episode_ids = ab_episode_catalog_ids(catalog),
                              .highscores = &highscores,
                              .star_table = stars.data.globals};
    const AbDataValue *record = ab_data_get(
        episodes, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)});
    if (!record || record->kind != AB_DATA_TABLE) {
        ab_fail(error, "Missing real APK first-episode metadata");
        goto release;
    }
    AbUiEpisodeButtonHost host = {.episodes = episodes,
                                  .fonts = fonts,
                                  .scaling = &menu->scaling,
                                  .random = &menu->random,
                                  .context = &inputs,
                                  .stars = episode_stars,
                                  .score = episode_score,
                                  .feathers = episode_feathers,
                                  .show_eagle = episode_eagle,
                                  .complete = episode_complete,
                                  .newest_change_seen = episode_seen,
                                  .has_new_levels = episode_new_levels,
                                  .new_levels_page = episode_page,
                                  .page_visited = episode_visited};
    if (!ab_ui_episode_button_init(&button, &menu->context, NULL, host, error))
        goto release;
    if (button.widget.frame->child_count != 8 ||
        button.frames[0] != &button.lock.tree) {
        ab_fail(error, "EpisodeButton actual constructor child graph differs");
        goto release;
    }
    button.widget.frame->fields.name = menu_text("episode1");
    button.widget.frame->fields.x = menu_number(240);
    button.widget.frame->fields.y = menu_number(140);
    button.widget.state.return_value = menu_text("GOTO_LEVEL_SELECTION");
    button.widget.state.disabled_return_value = menu_text("LOCKED_LEVEL_SELECTION");
    if (!ab_ui_episode_button_set_episode(&button, menu_number(1), record->table,
                                          error) ||
        strcmp(inputs.calls, "CVN") ||
        !ab_ui_episode_button_set_open(
            &button, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN}, error))
        goto release;
    inputs.count = 0;
    if (!ab_ui_widget_lifecycle(&button.widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    if (strcmp(inputs.calls, "SQECVN") ||
        !menu_equal(button.score.state.fields.text, "31480") ||
        !menu_equal(button.stars.state.fields.text, "2/189")) {
        ab_fail(error, "EpisodeButton score/query ordering differs");
        goto release;
    }
    inputs.count = 0;
    if (!ab_ui_widget_lifecycle(&button.widget, AB_UI_FRAME_LAYOUT, error) ||
        strcmp(inputs.calls, "CVNE") ||
        !ab_ui_pointer_value_truthy(&button.lock.frame->fields.visible))
        goto release;
    menu->render.clip = (AbDrawClip){0, 0, 480, 320, true};
    ab_draw_list_clear(menu->context.draw_list);
    AbUiDrawInput phase = {
        .transform = {.scale = {NAN, NAN}, .angle = 0, .local_factor = NAN},
        .present = 63};
    const char *const phases[] = {"drawBackground", "drawMidground", "drawForeground"};
    size_t counts[3];
    for (size_t index = 0; index < 3; ++index) {
        if (!ab_ui_widget_draw_named(&button.widget, menu_span(phases[index]), &phase,
                                     error))
            goto release;
        counts[index] = menu->context.draw_list->count;
        phase.transform.angle = NAN;
    }
    if (!(counts[0] && counts[1] > counts[0] && counts[2] > counts[1]) ||
        !ab_metal_capture(renderer, menu->context.draw_list, 480, 320, clear,
                          1024 * 1024, &captured, error) ||
        (path && !save(path, &captured, error)))
        goto release;
    if (!episode_action(menu, &button, "LOCKED_LEVEL_SELECTION", error) ||
        !ab_ui_episode_button_set_open(
            &button, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
            error) ||
        !episode_action(menu, &button, "GOTO_LEVEL_SELECTION", error))
        goto release;
    inputs.seen = false;
    inputs.visited = true;
    inputs.count = 0;
    uint64_t before = menu->random.value;
    if (!ab_ui_episode_button_new_sign(&button, error) ||
        strcmp(inputs.calls, "CVNPT") || menu->random.value == before ||
        !ab_ui_pointer_value_truthy(&button.new_sign.frame->fields.visible))
        goto release;
    button.new_timer = menu_number(0);
    if (!ab_ui_widget_update(&button.widget, 0.1f, 0.1f, error))
        goto release;
    float pulse = menu_real(button.new_sign.frame->fields.scale_x.number_bits);
    if (!(pulse > 0.85f && pulse < 1) ||
        button.new_sign.frame->fields.scale_x.number_bits !=
            button.new_sign.frame->fields.scale_y.number_bits)
        goto release;
    button.new_timer = menu_number(0.2f);
    if (!ab_ui_widget_update(&button.widget, 0, 0.2f, error) ||
        button.new_sign.frame->fields.scale_x.number_bits != UINT32_C(0x3f800000))
        goto release;
    inputs.eagle = true;
    inputs.seen = true;
    button.host.pc_profile = true;
    if (!ab_ui_episode_button_update_scores(&button, error) ||
        ab_ui_pointer_value_truthy(&button.feather_box.frame->fields.visible) ||
        !menu_equal(button.feather_score.state.fields.text, "1/63") ||
        !ab_ui_widget_lifecycle(&button.widget, AB_UI_FRAME_LAYOUT, error) ||
        !ab_ui_pointer_value_truthy(&button.feather_box.frame->fields.visible))
        goto release;
    /* Layout deliberately reapplies the eagle query after the PC score gate. */
    before = menu->random.value;
    size_t calls = inputs.count;
    button.disable_new_sign =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    if (!ab_ui_episode_button_new_sign(&button, error) || inputs.count != calls ||
        menu->random.value != before ||
        ab_ui_pointer_value_truthy(&button.new_sign.frame->fields.visible))
        goto release;
    printf("Actual APK EpisodeButton: eight children, phase counts %zu/%zu/%zu, "
           "locked/open input, ordered score/settings queries and new-sign pulse.\n",
           counts[0], counts[1], counts[2]);
    if (!episode_services_widgets(menu, &button, ab_episode_catalog_ids(catalog),
                                  &inputs.statistics, renderer, clear, first_path,
                                  last_path, error))
        goto release;
    okay = true;

release:
    if (!okay && !error->message[0])
        ab_fail(error, "EpisodeButton integration failed; queries: %s", inputs.calls);
    if (entered && !menu->context.failed &&
        !ab_ui_widget_lifecycle(&button.widget, AB_UI_FRAME_EXIT, error))
        okay = false;
    menu->render.clip = (AbDrawClip){0};
    ab_image_free(&captured);
    ab_episode_catalog_close(catalog);
    ab_target_data_close(&stars);
    return okay;
}

#endif
