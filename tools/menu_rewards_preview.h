#ifndef AB_MENU_REWARDS_PREVIEW_H
#define AB_MENU_REWARDS_PREVIEW_H

#include "ab/process_manager.h"

typedef struct {
    PreviewReward *reward;
    size_t phase;
    AbDataEntry fields[7];
    AbDataTable table, meta;
    AbProcessAction action;
} RewardAction;

struct PreviewReward {
    PreviewMenu *menu;
    AbUiWidget glow, egg;
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    AbProcessManager *process;
    RewardAction actions[3];
    AbProcessAction *bindings[3];
    AbDataValue finished, skip_input;
    float rate;
};

static void menu_reward_free(PreviewMenu *menu) {
    PreviewReward *reward = menu->reward;
    if (!reward)
        return;
    ab_process_manager_close(reward->process);
    if (reward->glow.initialized && !menu->context.failed) {
        AbError ignored = {0};
        (void)ab_ui_widget_lifecycle(&reward->glow, AB_UI_FRAME_EXIT, &ignored);
    }
    free(reward);
    menu->reward = NULL;
}

/* showRewardPopup94726's three actual callbacks and ProcessManager timing.
 * The app owns this notification until the process has completed; the original
 * offscreen final image has zero shade and no remaining input requirement. */
static bool reward_action(void *context, AbDataValue item, float elapsed,
                          float duration, float seconds, AbDataValue meta,
                          AbError *error) {
    (void)meta;
    RewardAction *action = context;
    PreviewReward *reward = action->reward;
    if (item.kind != AB_DATA_OBJECT || item.object != &reward->glow || duration == 0)
        return ab_fail(error, "Invalid golden-egg reward action identity/timing");
    float amount = elapsed / duration, eased = amount;
    AbUiFrameFields *glow = &reward->glow.frame->fields;
    float angle = menu_real(glow->angle.number_bits) + seconds * 2;
    if (!isfinite(amount) || !isfinite(angle))
        return ab_fail(error, "Golden-egg reward action overflow");
    if (action->phase == 0) {
        if (!ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_OUT, elapsed, 0, 1, duration,
                             &eased))
            return ab_fail(error, "Golden-egg reward rise tween failed");
        glow->y = menu_number((2.5f - eased * 1.5f) * (reward->menu->screen.y * 0.5f));
        glow->visible = reward->egg.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
        reward->rate = amount;
    } else if (action->phase == 1) {
        glow->y = menu_number(reward->menu->screen.y * 0.5f);
    } else {
        eased = (amount * amount) * amount;
        glow->y = menu_number(reward->menu->screen.y * 0.5f -
                              (eased * 0.7f) * reward->menu->screen.y);
        reward->rate = 1 - amount;
    }
    glow->x = menu_number(reward->menu->screen.x * 0.5f);
    glow->angle = menu_number(angle);
    reward->egg.frame->fields.angle = menu_number(-angle);
    return true;
}

static bool reward_process(PreviewReward *reward, AbError *error) {
    static const char *const names[] = {"golden egg rise", "golden egg stay middle",
                                        "golden egg go away"};
    for (size_t index = 0; index < 3; ++index) {
        RewardAction *action = reward->actions + index;
        action->reward = reward;
        action->phase = index;
        AbDataEntry fields[] = {
            {menu_data("name"), menu_data(names[index])},
            {menu_data("finished"), {.kind = AB_DATA_BOOLEAN}},
            {menu_data("item"), {.kind = AB_DATA_OBJECT, .object = &reward->glow}},
            {menu_data("start"), menu_route_number((float)index * 1.2f)},
            {menu_data("duration"), menu_route_number(1.2f)},
            {menu_data("meta"), {.kind = AB_DATA_TABLE, .table = &action->meta}},
            {menu_data("action"), {.kind = AB_DATA_OBJECT, .object = action}}};
        memcpy(action->fields, fields, sizeof(fields));
        action->table = (AbDataTable){action->fields, 7, 7, NULL};
        action->action = (AbProcessAction){
            .table = &action->table, .context = action, .action = reward_action};
        reward->bindings[index] = &action->action;
    }
    reward->finished = menu_event(reward->menu, "EID_PROCESS_FINISHED");
    if (!ab_process_manager_open(
            (AbProcessManagerConfig){.events = reward->menu->events,
                                     .finished_event = &reward->finished,
                                     .bindings = reward->bindings,
                                     .binding_count = 3,
                                     .actions = 3,
                                     .generations = 2,
                                     .notifications = 3,
                                     .storage_bytes = 16384},
            NULL, &reward->process, error))
        return false;
    for (size_t index = 0; index < 3; ++index)
        if (!ab_process_manager_insert(reward->process, reward->bindings[index], error))
            return false;
    return ab_process_manager_start(reward->process, error);
}

static bool menu_reward_reveal(PreviewMenu *menu, AbDataTable *event, AbError *error) {
    AbDataValue level = menu_route_field(event, "levelName");
    if (menu->reward || level.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported overlapping or unnamed menu reward");
    /* revealGoldenEgg96805 unlocks/saves before constructing its notification. */
    if (!ab_settings_golden_egg_unlock(menu->highscores, level.string, error) ||
        !menu_preview_flush(menu, error))
        return false;
    PreviewReward *reward = calloc(1, sizeof(*reward));
    if (!reward)
        return ab_fail(error, "Cannot allocate golden-egg notification");
    menu->reward = reward;
    reward->menu = menu;
    AbUiFrameFields arguments = {.visible = {.kind = AB_UI_POINTER_BOOLEAN}};
    if (!ab_ui_widget_init(
            &reward->glow, &menu->context, AB_UI_WIDGET_IMAGE,
            (AbUiWidgetChildren){reward->frames, reward->draw, reward->pointer, 1},
            NULL, &(AbUiImageArguments){.frame = arguments}, NULL, error) ||
        !ab_ui_image_state_set_image(&reward->glow.state.image,
                                     menu_text("GOLDEN_EGG_STAR_EFFECT"),
                                     &menu->context.images, error) ||
        !ab_ui_widget_init(&reward->egg, &menu->context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL,
                           &(AbUiImageArguments){.frame = arguments}, NULL, error) ||
        !ab_ui_image_state_set_image(&reward->egg.state.image,
                                     menu_text("GOLDEN_EGG_5"), &menu->context.images,
                                     error))
        return false;
    reward->egg.frame->fields.name = menu_text("egg");
    reward->glow.frame->fields.x = menu_number(menu->screen.x * 0.5f);
    reward->glow.frame->fields.y = menu_number(menu->screen.y * 0.9f);
    uint32_t handle;
    return ab_ui_widget_add(&reward->glow, &reward->egg, error) &&
           ab_audio_bank_play(menu->audio,
                              (AbAudioPlay){.name = menu_data("goldenegg"),
                                            .volume = menu_route_number(1),
                                            .loop = {.kind = AB_DATA_BOOLEAN}},
                              &handle, error) &&
           ab_ui_widget_lifecycle(&reward->glow, AB_UI_FRAME_ENTRY, error) &&
           ab_ui_scaling_set_to(&menu->scaling, &reward->glow.frame->fields, error) &&
           reward_process(reward, error);
}

static bool menu_reward_update(PreviewMenu *menu, float seconds, AbError *error) {
    PreviewReward *reward = menu->reward;
    if (!reward)
        return true;
    if (!ab_process_manager_update(reward->process, seconds, &reward->skip_input,
                                   error))
        return false;
    AbDataValue running =
        menu_route_field(ab_process_manager_state(reward->process), "running");
    if (!menu_route_truthy(running))
        menu_reward_free(menu);
    return true;
}

static bool menu_reward_draw(PreviewMenu *menu, AbError *error) {
    PreviewReward *reward = menu->reward;
    if (!reward)
        return true;
    AbGameRectanglePlan plan;
    AbDrawCommand command;
    bool visible;
    if (!ab_ui_scaling_set_to(&menu->scaling, &reward->glow.frame->fields, error) ||
        !ab_game_rectangle_plan(
            &(AbGameRectangle){.color = {0, 0, 0, reward->rate * 0.5f},
                               .right = menu->screen.x,
                               .bottom = menu->screen.y},
            &plan, error) ||
        !ab_fill_rectangle_reset_command(
            (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width, plan.height,
                              AB_FILL_BLEND_AUTO},
            (uint32_t)menu->screen.x, (uint32_t)menu->screen.y, 63, &command, &visible,
            error) ||
        (visible && !ab_draw_list_push(menu->context.draw_list, &command, error)))
        return false;
    return ab_ui_widget_draw(&reward->glow, &(AbUiDrawInput){0}, error);
}

#endif
