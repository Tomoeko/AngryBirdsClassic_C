#include "ab/ui_cutscene.h"

#include "ab/camera_mover.h"
#include "ab/fill_rectangle.h"
#include "ab/game_rectangle.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool equal(AbSpan left, AbSpan right) {
    return left.size == right.size &&
           (!left.size || !memcmp(left.data, right.data, left.size));
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span(value)};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static AbDataValue get(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool real(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Cutscene requires a numeric command field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite cutscene command field");
}

static bool named(AbDataValue value, const char *name) {
    return value.kind == AB_DATA_STRING && equal(value.string, span(name));
}

static bool bounds(AbUiCutscene *owner, AbDataValue image, AbVec2 *output,
                   AbError *error) {
    return image.kind == AB_DATA_STRING
               ? owner->widget.context->images.bounds(
                     owner->widget.context->images.context, image.string, output, error)
               : ab_fail(error, "Cutscene requires a real image identifier");
}

static bool cutscenes(AbUiCutscene *owner, const AbDataTable **output, AbError *error) {
    AbSpan path = span("cutscenesWatched");
    AbDataValue value =
        get(ab_settings_statistics_root(owner->host.settings), "cutscenesWatched");
    if (value.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(owner->host.settings, &path, 1, 0,
                                                 error))
            return false;
        value =
            get(ab_settings_statistics_root(owner->host.settings), "cutscenesWatched");
    }
    if (value.kind != AB_DATA_TABLE || !value.table)
        return ab_fail(error, "Cutscene watched settings are not a table");
    *output = value.table;
    return true;
}

static bool can_skip(AbUiCutscene *owner, bool *output, AbError *error) {
    const AbDataTable *watched = NULL;
    if (!cutscenes(owner, &watched, error))
        return false;
    const AbDataValue *value = ab_data_get(
        watched, (AbDataValue){.kind = AB_DATA_STRING, .string = owner->host.name});
    *output = value && value->kind == AB_DATA_BOOLEAN && value->bits;
    return true;
}

static bool mark_watched(AbUiCutscene *owner, AbError *error) {
    const AbDataTable *watched = NULL;
    AbSpan path[] = {span("cutscenesWatched"), owner->host.name};
    return cutscenes(owner, &watched, error) &&
           ab_settings_statistics_write(
               owner->host.settings, path, 2,
               (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error);
}

static bool layout(AbUiCutscene *owner, AbError *error) {
    AbUiScalingEnvironment view = {0};
    AbUiScaling *scaling = owner->host.scaling;
    if (!scaling->host.environment(scaling->host.context, &view, error))
        return false;
    float reference = owner->host.shared->reference_scale;
    owner->offset = (AbVec2){0};
    owner->scale = 1;
    owner->no_scroll = false;
    if (owner->fit_height) {
        float height = owner->reference.y * reference;
        if (view.screen.y < height)
            owner->scale = view.screen.y / height;
        else
            owner->offset.y = (view.screen.y - height) * 0.5f;
        float width = owner->reference.x * reference;
        if (width < view.screen.x) {
            owner->offset.x = (view.screen.x - width) * 0.5f;
            owner->no_scroll = true;
        }
    }
    AbVec2 size = {0};
    if (!ab_ui_scaling_sprite(scaling, &owner->widget.context->images,
                              span("BUTTON_SKIP_CUTSCENE"), false, &size, error) ||
        !ab_ui_scaling_set_to(scaling, &owner->skip.frame->fields, error))
        return false;
    owner->skip.frame->fields.x =
        number(view.screen.x - size.x * 0.55f - owner->host.margin.x);
    owner->skip.frame->fields.y =
        number(view.screen.y - size.y * 0.52f - owner->host.margin.y);
    owner->scale *= reference;
    if (!isfinite(owner->scale) || owner->scale <= 0)
        return ab_fail(error, "Unsupported cutscene layout scale");
    return ab_ui_widget_lifecycle_base(&owner->widget, AB_UI_FRAME_LAYOUT, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiCutscene *owner = context;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        owner->scroll = owner->offset = owner->position = (AbVec2){0};
        owner->scale = owner->multiplier.x = owner->multiplier.y = 1;
        owner->elapsed = 0;
        owner->command_index = 1;
        owner->sound_count = 0;
        owner->sprite_present = owner->fit_height = owner->no_scroll = false;
        if (!ab_ui_widget_lifecycle_base(widget, method, error) ||
            !ab_audio_mixer_all_action(owner->host.mixer, AB_AUDIO_STOP, error))
            return false;
        if (widget->context->left_button_held)
            owner->ldown = true;
        bool watched = false;
        if (!can_skip(owner, &watched, error))
            return false;
        /* Source class upvalue is initialized true: first-use skipping is
         * enabled while still calling the actual settings query above. */
        owner->skip.frame->fields.visible = boolean(true);
        return true;
    }
    return ab_ui_widget_lifecycle_base(widget, method, error);
}

static bool create_sprite(AbUiCutscene *owner, const AbDataTable *command,
                          AbError *error) {
    AbDataValue name = get(command, "name"), image = get(command, "image");
    if (name.kind != AB_DATA_STRING || image.kind != AB_DATA_STRING ||
        (owner->sprite_present && !equal(name.string, owner->sprite_name)))
        return ab_fail(error, "Cutscene requires the recovered single-sprite profile");
    const char *const transform[] = {"flip_x",  "flip_y", "scale_x",
                                     "scale_y", "angle",  "alpha"};
    for (size_t index = 0; index < sizeof(transform) / sizeof(transform[0]); ++index)
        if (get(command, transform[index]).kind != AB_DATA_NIL)
            return ab_fail(error,
                           "Cutscene sprite transform needs its full command binding");
    float x = 0, y = 0;
    if (!real(get(command, "x"), &x, error) || !real(get(command, "y"), &y, error))
        return false;
    owner->position = (AbVec2){x * owner->multiplier.x, y * owner->multiplier.y};
    owner->sprite_name = name.string;
    owner->image = image.string;
    owner->sprite_present = true;
    return true;
}

static bool begin_scroll(AbUiCutscene *owner, const AbDataTable *command,
                         AbError *error) {
    AbDataValue target = get(command, "scroll_target"),
                cursor = get(command, "scroll_cursor");
    if (!owner->sprite_present || target.kind != AB_DATA_TABLE || !target.table ||
        cursor.kind != AB_DATA_TABLE || !cursor.table ||
        !named(get(target.table, "type"), "sprite") ||
        !named((AbDataValue){.kind = AB_DATA_STRING, .string = owner->sprite_name},
               "background") ||
        get(target.table, "sprite").kind != AB_DATA_STRING ||
        !equal(get(target.table, "sprite").string, owner->sprite_name) ||
        truthy(get(command, "restricted_scroll_direction")))
        return ab_fail(error, "Unsupported cutscene scroll target/profile");
    AbVec2 size = {0}, view_cursor = {0}, target_point = {0};
    AbUiScalingEnvironment view = {0};
    if (!bounds(owner, (AbDataValue){.kind = AB_DATA_STRING, .string = owner->image},
                &size, error) ||
        !owner->host.scaling->host.environment(owner->host.scaling->host.context, &view,
                                               error) ||
        !real(get(target.table, "x"), &target_point.x, error) ||
        !real(get(target.table, "y"), &target_point.y, error) ||
        !real(get(cursor.table, "x"), &view_cursor.x, error) ||
        !real(get(cursor.table, "y"), &view_cursor.y, error))
        return false;
    float cursor_x = owner->scroll.x + view_cursor.x * (view.screen.x / owner->scale -
                                                        owner->offset.x * 2);
    float cursor_y = owner->scroll.y + view_cursor.y * (view.screen.y / owner->scale -
                                                        owner->offset.y * 2);
    float target_x = owner->position.x + target_point.x * size.x;
    float target_y = owner->position.y + target_point.y * size.y;
    owner->scroll_begin = owner->scroll;
    owner->scroll_end = (AbVec2){owner->scroll.x + (target_x - cursor_x),
                                 owner->scroll.y + (target_y - cursor_y)};
    float duration = 0;
    if (!real(get(command, "time"), &owner->scroll_begin_time, error) ||
        !real(get(command, "duration"), &duration, error))
        return false;
    owner->scroll_end_time = owner->scroll_begin_time + duration;
    AbDataValue kind = get(command, "type");
    owner->scroll_cubic = named(kind, "cubic_in_out");
    if (truthy(kind) && !owner->scroll_cubic && !named(kind, "linear"))
        return ab_fail(error, "Unsupported cutscene scroll curve");
    owner->scrolling = true;
    return true;
}

static bool process(AbUiCutscene *owner, const AbDataTable *command, AbError *error) {
    AbDataValue action = get(command, "action");
    if (named(action, "fitheight")) {
        owner->fit_height = true;
        return bounds(owner, get(command, "image"), &owner->reference, error) &&
               layout(owner, error);
    }
    if (named(action, "setreferencesize")) {
        AbVec2 size = {0}, reference = {0};
        if (!bounds(owner, get(command, "image"), &size, error) ||
            !real(get(command, "width"), &reference.x, error) ||
            !real(get(command, "height"), &reference.y, error))
            return false;
        if (reference.x == 0 || reference.y == 0)
            return ab_fail(error, "Cutscene reference dimensions must be nonzero");
        owner->multiplier = (AbVec2){size.x / reference.x, size.y / reference.y};
        return true;
    }
    if (named(action, "playsound")) {
        AbDataValue sound = get(command, "sound"), gain = get(command, "volume"),
                    loop = get(command, "loop");
        float volume = 1, track = 0;
        if (sound.kind != AB_DATA_STRING || owner->sound_count == 8)
            return ab_fail(error, "Unsupported cutscene resource audio arguments");
        if ((truthy(gain) && !real(gain, &volume, error)) ||
            !real(get(command, "track"), &track, error))
            return false;
        if (track < 0 || track > 7 || truncf(track) != track)
            return ab_fail(error, "Unsupported cutscene resource audio track");
        uint32_t handle;
        if (!ab_audio_bank_play_resource(owner->host.audio, sound.string, volume,
                                         truthy(loop), (uint32_t)track, &handle, error))
            return false;
        owner->sounds[owner->sound_count++] = sound.string;
        return true;
    }
    if (named(action, "createsprite"))
        return create_sprite(owner, command, error);
    if (named(action, "scroll"))
        return begin_scroll(owner, command, error);
    if (named(action, "end")) {
        if (!mark_watched(owner, error))
            return false;
        if (!owner->host.save_settings)
            return ab_fail(error,
                           "Cutscene end requires the actual settings save host");
        return owner->host.save_settings(owner->host.context, error) &&
               (!owner->host.end_event ||
                ab_event_manager_notify(owner->host.events, owner->host.end_event,
                                        error));
    }
    return ab_fail(error,
                   "Cutscene command remains outside the prepared intro profile");
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiCutscene *owner = context;
    owner->elapsed += seconds;
    if (!isfinite(owner->elapsed))
        return ab_fail(error, "Nonfinite cutscene elapsed time");
    for (; owner->command_index <= owner->host.commands->count;
         ++owner->command_index) {
        const AbDataValue *command = ab_data_get(
            owner->host.commands,
            (AbDataValue){.kind = AB_DATA_NUMBER,
                          .bits = number((float)owner->command_index).number_bits});
        float deadline = 0;
        if (!command || command->kind != AB_DATA_TABLE || !command->table)
            return ab_fail(error, "Missing cutscene command/time record");
        if (!real(get(command->table, "time"), &deadline, error))
            return false;
        if (owner->elapsed < deadline)
            break;
        if (!process(owner, command->table, error))
            return false;
    }
    if (owner->scrolling && !owner->no_scroll) {
        float duration = owner->scroll_end_time - owner->scroll_begin_time;
        float phase = owner->elapsed - owner->scroll_begin_time;
        float amount = 0;
        if (owner->scroll_cubic) {
            if (!ab_camera_tween_cubic(phase, 0, 1, duration, &amount))
                return ab_fail(error, "Unsupported cutscene scroll arithmetic");
        } else {
            amount = phase / duration;
        }
        if (!isfinite(amount))
            return ab_fail(error, "Nonfinite cutscene scroll amount");
        if (amount >= 1) {
            owner->scrolling = false;
            owner->scroll = owner->scroll_end;
        } else {
            owner->scroll.x = owner->scroll_begin.x +
                              amount * (owner->scroll_end.x - owner->scroll_begin.x);
            owner->scroll.y = owner->scroll_begin.y +
                              amount * (owner->scroll_end.y - owner->scroll_begin.y);
        }
    }
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

static void reset(AbUiImageRenderState *state, bool alpha) {
    bool direct = state->geometry.direct_vertices;
    state->geometry =
        (AbSpriteRenderState){.scale_x = 1, .scale_y = 1, .direct_vertices = direct};
    if (alpha)
        state->alpha = 1;
}

static bool fill(AbUiCutscene *owner, const float color[4], float left, float top,
                 float right, float bottom, AbError *error) {
    AbUiWidgetContext *context = owner->widget.context;
    AbGameRectangle rectangle = {
        .left = left, .top = top, .right = right, .bottom = bottom};
    memcpy(rectangle.color, color, sizeof(rectangle.color));
    AbGameRectanglePlan plan;
    AbDrawCommand command;
    bool visible = false;
    if (!ab_game_rectangle_plan(&rectangle, &plan, error))
        return false;
    reset(context->render_state, true);
    if (!ab_fill_rectangle_reset_command(
            (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width, plan.height,
                              AB_FILL_BLEND_AUTO},
            context->render.viewport_width, context->render.viewport_height,
            owner->host.white_texture, &command, &visible, error))
        return false;
    command.scissor = context->render_state->clip;
    return !visible || ab_draw_list_push(context->draw_list, &command, error);
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiCutscene *owner = context;
    AbUiWidgetContext *ui = widget->context;
    AbUiScalingEnvironment view = {0};
    if (!owner->host.scaling->host.environment(owner->host.scaling->host.context, &view,
                                               error))
        return false;
    float width = view.screen.x, height = view.screen.y;
    reset(ui->render_state, false);
    const float black[] = {0, 0, 0, 1};
    if (owner->offset.y > 0 &&
        (!fill(owner, black, 0, 0, ceilf(width), ceilf(owner->offset.y), error) ||
         !fill(owner, black, 0, floorf(height - owner->offset.y), ceilf(width),
               ceilf(height), error)))
        return false;
    if (owner->offset.x > 0 &&
        (!fill(owner, black, 0, 0, ceilf(owner->offset.x), ceilf(height), error) ||
         !fill(owner, black, floorf(width - owner->offset.x), 0, ceilf(width),
               ceilf(height), error) ||
         !ab_screen_clip((AbScreenClipRect){owner->offset.x, 0,
                                            width - owner->offset.x * 2, height},
                         ui->screen_clip_offset, &ui->render_state->clip, error)))
        return false;
    float background[] = {owner->color[0] / 255, owner->color[1] / 255,
                          owner->color[2] / 255, 1};
    if (!fill(owner, background, owner->offset.x, owner->offset.y,
              width - owner->offset.x, height - owner->offset.y, error))
        return false;
    if (owner->offset.x > 0 || owner->offset.y > 0) {
        float x = fmaxf(owner->offset.x, 0), y = fmaxf(owner->offset.y, 0);
        if (!ab_screen_clip((AbScreenClipRect){x, y, width - 2 * x, height - 2 * y},
                            ui->screen_clip_offset, &ui->render_state->clip, error))
            return false;
    }
    if (owner->sprite_present) {
        AbVec2 pivot = {0};
        if (!ui->images.pivot(ui->images.context, owner->image, &pivot, error))
            return false;
        ui->render_state->geometry.translation_x =
            -floorf(owner->scroll.x) +
            (owner->offset.x + owner->position.x * owner->scale) / owner->scale;
        ui->render_state->geometry.translation_y =
            -floorf(owner->scroll.y) +
            (owner->offset.y + owner->position.y * owner->scale) / owner->scale;
        ui->render_state->geometry.scale_x = ui->render_state->geometry.scale_y =
            owner->scale;
        ui->render_state->geometry.rotation_origin_x = pivot.x;
        ui->render_state->geometry.rotation_origin_y = pivot.y;
        ui->render_state->alpha = 1;
        AbUiSpriteRender sprite = {.image = owner->image,
                                   .vertical = AB_ANCHOR_VPIVOT,
                                   .horizontal = AB_ANCHOR_HPIVOT};
        if (!ab_ui_sprite_render(&sprite, ui->render_state, &ui->render, ui->draw_list,
                                 error))
            return false;
    }
    reset(ui->render_state, true);
    return ab_screen_clip((AbScreenClipRect){0, 0, width, height},
                          ui->screen_clip_offset, &ui->render_state->clip, error) &&
           ab_ui_widget_draw_base(widget, input, error);
}

bool ab_ui_cutscene_skip(AbUiCutscene *owner, AbError *error) {
    if (!owner || !owner->widget.initialized || owner->widget.context->failed)
        return ab_fail(error, "Invalid cutscene skip owner");
    for (size_t index = 0; index < owner->sound_count; ++index)
        if (!ab_audio_bank_stop(owner->host.audio, owner->sounds[index], error))
            return false;
    if (!mark_watched(owner, error))
        return false;
    return owner->host.end_event
               ? ab_event_manager_notify(owner->host.events, owner->host.end_event,
                                         error)
               : ab_fail(error, "Cutscene skip requires its actual completion event");
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan action, AbVec2 point,
                    AbUiPointerReply *reply, AbError *error) {
    AbUiCutscene *owner = context;
    if (!ab_ui_widget_pointer_base(widget, action, point, reply, error))
        return false;
    owner->trigger_skip = equal(action, span("LRELEASE")) &&
                          reply->values[0].kind == AB_UI_POINTER_TEXT &&
                          equal(reply->values[0].text, span("SKIP_TO_LEVEL"));
    bool watched = false;
    if (!can_skip(owner, &watched, error))
        return false;
    owner->skip_allowed = true;
    if (owner->trigger_skip && owner->skip_allowed) {
        if (owner->ldown)
            owner->ldown = false;
        else if (!ab_ui_cutscene_skip(owner, error))
            return false;
    }
    return true;
}

bool ab_ui_cutscene_init(AbUiCutscene *owner, AbUiWidgetContext *context,
                         AbUiCutsceneHost host, AbError *error) {
    if (!owner || owner->widget.initialized || !context || !host.scaling ||
        !host.shared || !host.audio || !host.mixer || !host.settings || !host.events ||
        !host.commands || host.commands->count > host.commands->capacity ||
        (host.commands->count && !host.commands->entries) ||
        host.commands->count > 64 || !host.name.size || !host.name.data ||
        !host.white_texture)
        return ab_fail(error, "Invalid cutscene constructor host/storage");
    owner->host = host;
    AbUiFrameFields frame = {.name = text("CutScene")};
    if (!ab_ui_widget_init(
            &owner->widget, context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){owner->frames, owner->draw, owner->pointer, 1}, &frame,
            NULL, NULL, error))
        return false;
    owner->widget.methods =
        (AbUiWidgetMethods){owner, lifecycle, draw, pointer, update, NULL};
    AbUiScalingEnvironment view = {0};
    if (!host.scaling->host.environment(host.scaling->host.context, &view, error))
        return false;
    if (view.screen.x <= 1024 && equal(host.device, span("android")))
        host.shared->reference_scale = view.screen.x / 480;
    else if (!ab_ui_scaling_get(host.scaling, &host.shared->reference_scale, error))
        return false;
    AbUiImageButtonArguments skip = {.image.frame.name = text("skip_button"),
                                     .return_value = text("SKIP_TO_LEVEL"),
                                     .activate_on_release = boolean(true),
                                     .click_sound = text("menu_confirm")};
    if (!ab_ui_widget_init(&owner->skip, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, &skip, error) ||
        !ab_ui_image_button_state_set_image(
            &owner->skip.state, text("BUTTON_SKIP_CUTSCENE"), (AbUiPointerValue){0},
            &context->images, error))
        return false;
    owner->skip.frame->fields.visible = boolean(false);
    return ab_ui_widget_add(&owner->widget, &owner->skip, error);
}
