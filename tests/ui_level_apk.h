#ifndef AB_UI_LEVEL_APK_TEST_H
#define AB_UI_LEVEL_APK_TEST_H

#include "ab/level_progress.h"
#include "ab/ui_level_button.h"
#include "ab/ui_level_services.h"

static const AbDataTable *level_test_child(const AbDataTable *parent, AbDataValue key) {
    const AbDataValue *value = ab_data_get(parent, key);
    return value && value->kind == AB_DATA_TABLE ? value->table : NULL;
}

static AbDataValue level_test_number(size_t index) {
    return (AbDataValue){.kind = AB_DATA_NUMBER,
                         .bits = menu_number((float)index).number_bits};
}

static bool level_test_click(PreviewMenu *menu, AbUiLevelButton *button, bool open,
                             AbError *error) {
    AbVec2 point, scale, size, pivot;
    AbUiPointerValue image = button->widget.state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_widget_screen_position(&button->widget, &point, error) ||
        !ab_ui_widget_cumulative_scale(&button->widget, &scale, error) ||
        !menu->context.images.bounds(menu->context.images.context, image.text, &size,
                                     error) ||
        !menu->context.images.pivot(menu->context.images.context, image.text, &pivot,
                                    error))
        return false;
    point.x += (size.x * 0.5f - pivot.x) * scale.x;
    point.y += (size.y * 0.5f - pivot.y) * scale.y;
    menu->context.cursor = point;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&button->widget, menu_span("LPRESS"), point, &reply,
                              error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    if (!ab_ui_widget_pointer(&button->widget, menu_span("LRELEASE"), point, &reply,
                              error))
        return false;
    return (menu_equal(reply.values[0], open ? "GOTO_LEVEL" : "DISABLED_LEVEL") &&
            reply.values[1].kind == AB_UI_POINTER_OBJECT &&
            reply.values[1].object == (uintptr_t)&button->meta &&
            reply.values[2].object == (uintptr_t)&button->widget) ||
           ab_fail(error, "Actual level-button release reply/meta differs");
}

static bool level_reward_widgets(PreviewMenu *menu, AbError *error) {
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    const AbDataTable *episode = level_test_child(episodes, level_test_number(1));
    const AbDataTable *pages = level_test_child(episode, menu_data("pages"));
    const AbDataTable *page = level_test_child(pages, level_test_number(1));
    const AbDataTable *levels = level_test_child(page, menu_data("levels"));
    const AbDataTable *level = level_test_child(levels, level_test_number(1));
    const AbDataValue *name = ab_data_get(level, menu_data("name"));
    if (!name)
        return ab_fail(error, "Missing real reward-fixture level name");
    const AbDataTable *limits = level_test_child(menu->star_limits.data.globals, *name);
    const AbDataValue *gold = ab_data_get(limits, menu_data("goldScore"));
    if (!gold)
        return ab_fail(error, "Missing real reward-fixture gold threshold");
    AbDataEntry score_fields[] = {
        {menu_data("score"), *gold},
        {menu_data("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {menu_data("eagleScore"), level_test_number(100)}};
    AbDataTable score = {score_fields, 3, 3, NULL};
    AbDataEntry saved = {*name, {.kind = AB_DATA_TABLE, .table = &score}};
    AbDataTable highscores = {&saved, 1, 1, NULL};
    AbDataEntry used_field = {menu_data("level"), *name};
    AbDataTable used_record = {&used_field, 1, 1, NULL};
    AbDataEntry used_item = {level_test_number(1),
                             {.kind = AB_DATA_TABLE, .table = &used_record}};
    AbDataTable used = {&used_item, 1, 1, NULL};
    AbDataEntry local_field = {menu_data("eaglesUsedIn"),
                               {.kind = AB_DATA_TABLE, .table = &used}};
    AbDataTable local = {&local_field, 1, 1, NULL};
    AbEpisodeStatistics statistics = menu->statistics;
    statistics.highscores = &highscores;
    AbUiLevelServices services = {.statistics = &statistics, .local_settings = &local};
    AbUiLevelButtonHost host = ab_ui_level_services_host(&services);
    host.fonts = &menu->fonts;
    host.scaling = &menu->scaling;
    AbUiLevelButton button = {0};
    if (!ab_ui_level_button_init(&button, &menu->context, level, host, error) ||
        !ab_ui_level_button_open(&button, error))
        return false;
    if (!menu_equal(button.stars.state.image.image, "LS_STARS_3") ||
        !ab_ui_pointer_value_truthy(&button.stars.frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&button.eagle.frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(&button.feather.frame->fields.visible))
        return ab_fail(error, "Completed level star/feather images differ");
    score_fields[0].value = level_test_number(0);
    if (!ab_ui_level_button_open(&button, error) ||
        !ab_ui_pointer_value_truthy(&button.eagle.frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&button.stars.frame->fields.visible))
        return ab_fail(error, "Mighty Eagle completion did not use real saved history");
    local_field.value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (!ab_ui_level_button_close(&button, error))
        return false;
    button.has_video_button = menu_number(0);
    if (!ab_ui_level_button_open(&button, error) ||
        ab_ui_pointer_value_truthy(&button.eagle.frame->fields.visible) ||
        ab_ui_pointer_value_truthy(&button.number.widget.frame->fields.visible))
        return ab_fail(error, "False eagle history/video-button visibility differs");
    AbUiWidget stars_empty = {0};
    AbUiImageArguments empty_arguments = {.frame.name = menu_text("starsEmpty")};
    if (!ab_ui_widget_init(&stars_empty, &menu->context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, &empty_arguments, NULL,
                           error) ||
        !ab_ui_widget_add(&button.widget, &stars_empty, error))
        return false;
    button.stars_empty = &stars_empty;
    if (!ab_ui_level_button_open(&button, error) ||
        !ab_ui_pointer_value_truthy(&stars_empty.frame->fields.visible) ||
        !ab_ui_level_button_close(&button, error) ||
        ab_ui_pointer_value_truthy(&stars_empty.frame->fields.visible))
        return ab_fail(error, "Actual optional starsEmpty child did not open/close");
    printf("Real level reward UI: three stars, eagle history, perfect feather, "
           "video-button visibility and optional subclass child.\n");
    return true;
}

static bool level_grid_widgets(PreviewMenu *menu, AbMetal *renderer, const char *path,
                               AbError *error) {
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    AbLevelProgress progress = {
        .episodes = episodes, .highscores = &menu->empty, .release_build = true};
    AbUiLevelButtonHost host = {.episodes = episodes,
                                .statistics = &menu->statistics,
                                .scaling = &menu->scaling,
                                .fonts = &menu->fonts};
    if (!level_reward_widgets(menu, error))
        return false;
    size_t checked = 0, checked_pages = 0;
    AbUiLevelButton buttons[21];
    for (size_t episode = 0; episode < episodes->count; ++episode) {
        const AbDataTable *record = episodes->entries[episode].value.table;
        const AbDataTable *pages = level_test_child(record, menu_data("pages"));
        for (size_t page_index = 1; pages && page_index <= pages->count; ++page_index) {
            const AbDataTable *page =
                level_test_child(pages, level_test_number(page_index));
            const AbDataValue *layout = ab_data_get(page, menu_data("layout"));
            if (!layout || layout->kind != AB_DATA_STRING || layout->string.size != 4 ||
                memcmp(layout->string.data, "grid", 4))
                continue;
            const AbDataTable *levels = level_test_child(page, menu_data("levels"));
            if (!levels || levels->count > 21)
                return ab_fail(error, "Actual grid page exceeds test workspace");
            memset(buttons, 0, sizeof(buttons));
            ab_draw_list_clear(menu->context.draw_list);
            for (size_t index = 0; index < levels->count; ++index) {
                const AbDataTable *level =
                    level_test_child(levels, level_test_number(index + 1));
                AbUiLevelButton *button = buttons + index;
                bool open;
                if (!ab_ui_level_button_init(button, &menu->context, level, host,
                                             error) ||
                    !ab_ui_widget_lifecycle(&button->widget, AB_UI_FRAME_ENTRY,
                                            error) ||
                    !ab_ui_level_button_grid_layout(button, true, (AbVec2){0}, error) ||
                    !ab_level_progress_grid_open(&progress, level, &open, error) ||
                    !(open ? ab_ui_level_button_open(button, error)
                           : ab_ui_level_button_close(button, error)) ||
                    !level_test_click(menu, button, open, error))
                    return false;
                AbSpriteAtlasBinding binding;
                AbResolvedComposite composite;
                const AbSpriteProvider *sprites = &menu->sprites.provider;
                AbUiPointerValue selected = button->widget.state.image.image;
                if (button->widget.frame->child_count != 4 ||
                    selected.kind != AB_UI_POINTER_TEXT ||
                    (!sprites->sprite(sprites->context, selected.text, &binding) &&
                     (!sprites->composite ||
                      !sprites->composite(sprites->context, selected.text,
                                          &composite))))
                    return ab_fail(
                        error, "Level %zu/%zu has %zu children and missing art %.*s",
                        page_index, index + 1, button->widget.frame->child_count,
                        (int)selected.text.size, selected.text.data);
                AbUiDrawInput input = {0};
                if (!ab_ui_widget_draw(&button->widget, &input, error))
                    return false;
                ++checked;
            }
            ++checked_pages;
            if (episode == 0 && page_index == 1) {
                AbImage captured = {0};
                const float clear[] = {41.0f / 255, 118.0f / 255, 142.0f / 255, 1};
                bool okay =
                    ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                     clear, 16 * 1024 * 1024, &captured, error) &&
                    (!path || save(path, &captured, error));
                ab_image_free(&captured);
                if (!okay)
                    return false;
            }
            for (size_t index = 0; index < levels->count; ++index)
                if (!ab_ui_widget_lifecycle(&buttons[index].widget, AB_UI_FRAME_EXIT,
                                            error))
                    return false;
        }
    }
    printf(
        "Actual APK basic grids: %zu levels on %zu pages, real images/fonts, "
        "progression gates, release metadata and source layout; first page on Metal.\n",
        checked, checked_pages);
    return true;
}

#endif
