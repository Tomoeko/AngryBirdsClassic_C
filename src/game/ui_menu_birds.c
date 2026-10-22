#include "ab/ui_menu_birds.h"

#include "ab/script_math.h"
#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

static AbSpan text(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = text(name)};
}

static AbDataValue get(const AbDataTable *table, AbDataValue name) {
    const AbDataValue *value = ab_data_get(table, name);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, key(name));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbSpan name, const char *literal) {
    return name.size == strlen(literal) &&
           (!name.size || (name.data && !memcmp(name.data, literal, name.size)));
}

static AbDataValue index_value(size_t index) {
    float value = (float)index;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool valid(const AbUiMenuBirds *owner, AbVec2 screen, AbError *error) {
    return owner && owner->initialized && owner->random &&
                   owner->candidate_count <= AB_UI_MENU_BIRD_CAPACITY &&
                   owner->actor_count <= AB_UI_MENU_BIRD_CAPACITY &&
                   isfinite(screen.x) && isfinite(screen.y) && screen.x > 0 &&
                   screen.y > 0
               ? true
               : ab_fail(error, "Invalid menu bird owner or screen");
}

bool ab_ui_menu_birds_init(AbUiMenuBirds *owner, AbRandom48 *random, AbError *error) {
    if (!owner || owner->initialized || !random)
        return ab_fail(error, "Invalid menu bird initialization");
    owner->random = random;
    owner->initialized = true;
    return true;
}

static bool add(AbUiMenuBirds *owner, AbSpan sprite, unsigned reward, float scale,
                AbError *error) {
    if ((sprite.size && !sprite.data) ||
        owner->candidate_count == AB_UI_MENU_BIRD_CAPACITY)
        return ab_fail(error, "Menu bird candidate storage or name is invalid");
    owner->candidates[owner->candidate_count++] =
        (AbUiMenuBirdCandidate){sprite, scale, reward};
    return true;
}

static bool three_starred(const AbDataTable *settings, AbDataValue episode,
                          bool *output, AbError *error) {
    AbUiPointerValue identifier = {0};
    if (episode.kind == AB_DATA_STRING)
        identifier =
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = episode.string};
    else if (episode.kind == AB_DATA_NUMBER)
        identifier = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                        .number_bits = episode.bits};
    else
        return ab_fail(error, "Invalid menu reward episode identifier");
    uint8_t storage[96], temporary[32];
    AbSpan suffix;
    if (!ab_ui_value_text(identifier, temporary, sizeof(temporary), &suffix) ||
        suffix.size > sizeof(storage) - 12)
        return ab_fail(error, "Menu reward settings key exceeds storage");
    memcpy(storage, "threeStarsLP", 12);
    if (suffix.size)
        memcpy(storage + 12, suffix.data, suffix.size);
    *output =
        truthy(get(settings, (AbDataValue){.kind = AB_DATA_STRING,
                                           .string = {storage, 12 + suffix.size}}));
    return true;
}

static bool reward(AbUiMenuBirds *owner, AbDataValue record, float scale,
                   AbError *error) {
    if (record.kind != AB_DATA_TABLE || !record.table)
        return ab_fail(error, "Malformed menu reward metadata");
    AbDataValue sprite = field(record.table, "sprite");
    AbDataValue type = field(record.table, "type");
    if (sprite.kind != AB_DATA_STRING || type.kind != AB_DATA_STRING)
        return ab_fail(error, "Missing menu reward sprite/type");
    unsigned kind = named(type.string, "projectile") ? 1
                    : named(type.string, "balloon")  ? 2
                                                     : 0;
    if (!kind)
        return ab_fail(error, "Unsupported menu reward type");
    return add(owner, sprite.string, kind, scale, error) &&
           add(owner, sprite.string, kind, scale, error);
}

bool ab_ui_menu_birds_build(AbUiMenuBirds *owner, AbSettingsStatistics *settings,
                            const AbEpisodeStatistics *statistics,
                            const AbDataTable *additional_feather_episodes,
                            AbVec2 screen, AbSpan device_model, bool retina_ipad,
                            AbError *error) {
    if (!valid(owner, screen, error) || !settings || !statistics ||
        !statistics->episode_ids || statistics->episode_ids->count > UINT32_C(16777216))
        return ab_fail(error, "Invalid menu bird-list producers");
    float reward_scale =
        ((named(device_model, "iphone") && screen.x >= 960) || retina_ipad) ? 0.5f : 1;
    owner->candidate_count = 0;
    const AbDataTable *root = ab_settings_statistics_root(settings);
    AbSpan path = text("tutorials");
    if (field(root, "tutorials").kind == AB_DATA_NIL &&
        !ab_settings_statistics_create_table(settings, &path, 1, 0, error))
        return false;
    size_t count;
    if (!ab_settings_statistics_pairs(settings, &path, 1, owner->tutorials,
                                      AB_UI_MENU_BIRD_CAPACITY, &count, error))
        return false;
    for (size_t index = 0; index < count; ++index) {
        AbSpan sprite = owner->tutorials[index].key.string;
        if (named(sprite, "PORTALBLUE_1"))
            continue;
        bool sardine = named(sprite, "BAIT_SARDINE");
        if (sardine && !truthy(field(root, "mightyEagleUnlocked")))
            continue;
        if (!add(owner, sprite, sardine ? 1 : 0, 1, error))
            return false;
    }
    if (!owner->candidate_count && !add(owner, text("BIRD_RED"), 0, 1, error))
        return false;
    for (size_t index = 1; index <= statistics->episode_ids->count; ++index) {
        AbDataValue id = get(statistics->episode_ids, index_value(index));
        AbDataValue record = get(statistics->episodes, id);
        if (record.kind != AB_DATA_TABLE || !record.table)
            return ab_fail(error, "Missing menu reward episode");
        AbDataValue stars = field(record.table, "reward_three_stars");
        AbDataValue complete = field(record.table, "reward_completion");
        bool starred = false, completed;
        if (truthy(stars) && !three_starred(root, id, &starred, error))
            return false;
        if (starred) {
            if (!reward(owner, stars, reward_scale, error))
                return false;
        } else if (truthy(complete)) {
            if (!ab_episode_statistics_all_complete(statistics, record.table,
                                                    &completed, error))
                return false;
            if (completed && !reward(owner, complete, reward_scale, error))
                return false;
        }
    }
    float feathers, available;
    if (!ab_episode_statistics_all_feathers(statistics, additional_feather_episodes,
                                            &feathers, &available, error))
        return false;
    return feathers < 200 ||
           (add(owner, text("REWARD_EAGLE"), 1, reward_scale, error) &&
            add(owner, text("REWARD_EAGLE"), 1, reward_scale, error));
}

static bool integer_argument(float value, int32_t *output, AbError *error) {
    if (!isfinite(value) || (double)value < -2147483648.0 ||
        (double)value >= 2147483648.0) {
        ab_fail(error, "Menu bird random argument exceeds signed32 domain");
        return false;
    }
    *output = (int32_t)value;
    return true;
}

static bool random_value(AbUiMenuBirds *owner, unsigned count, float first,
                         float second, float *output, AbError *error) {
    int32_t a = 0, b = 0;
    if ((count && !integer_argument(first, &a, error)) ||
        (count == 2 && !integer_argument(second, &b, error)))
        return false;
    return ab_script_random(owner->random, count, a, b, output) ==
               AB_SCRIPT_RANDOM_OK ||
           ab_fail(error, "Menu bird random interval or stream is invalid");
}

static bool yell(AbUiMenuBirdActor *actor, AbError *error) {
    AbSpan name = actor->sprite;
    const char *exception = named(name, "BIRD_PUFFER_1")       ? "BIRD_PUFFER_YELL"
                            : named(name, "BIRD_PINK")         ? "BIRD_PINK_BLINK"
                            : named(name, "BIRD_RED_FEATHERS") ? "BIRD_RED_YELL"
                                                               : NULL;
    if (exception)
        name = text(exception);
    size_t suffix = exception ? 0 : 5;
    if (name.size > sizeof(actor->yell) - suffix)
        return ab_fail(error, "Menu bird yell identifier exceeds storage");
    if (name.size)
        memcpy(actor->yell, name.data, name.size);
    if (suffix)
        memcpy(actor->yell + name.size, "_YELL", suffix);
    actor->yell_size = name.size + suffix;
    return true;
}

static bool spawn(AbUiMenuBirds *owner, AbUiWidgetContext *context, AbVec2 screen,
                  AbError *error) {
    AbUiMenuBirdActor actor = {0};
    float layer, selected, x_speed, y_speed;
    if (!random_value(owner, 2, 3, 5, &layer, error) ||
        !random_value(owner, 2, -screen.x * 0.75f, screen.x * 0.75f, &actor.position.x,
                      error))
        return false;
    actor.position.y = screen.y + (30 * screen.y) / 320;
    if (!random_value(owner, 1, (float)owner->candidate_count, 0, &selected, error))
        return false;
    if (selected < 1 || selected > (float)owner->candidate_count ||
        truncf(selected) != selected)
        return ab_fail(error, "Menu bird random index is outside its source list");
    const AbUiMenuBirdCandidate *candidate = owner->candidates + (size_t)selected - 1;
    actor.sprite = candidate->sprite;
    actor.reward = candidate->reward;
    actor.layer = (unsigned)layer;
    actor.scale = (layer * 0.2f) * candidate->scale;
    if (!isfinite(actor.scale) || actor.scale <= 0)
        return ab_fail(error, "Invalid menu bird candidate scale");
    if (!random_value(owner, 2, 100, 350, &x_speed, error) ||
        !random_value(owner, 2, -400, -150, &y_speed, error))
        return false;
    actor.velocity.x = ((x_speed * actor.scale) * (screen.x / 480 + 1)) / 2;
    actor.velocity.y = ((y_speed * actor.scale) * (screen.y / 320 + 1)) / 2;
    actor.position.y += ((screen.y / 320) * 20) * (5 - layer);
    if (layer == 1) {
        actor.velocity.y *= 1.75f;
        actor.velocity.x *= 1.75f;
    }
    AbVec2 bounds;
    if (!context->images.bounds(context->images.context, actor.sprite, &bounds, error))
        return false;
    actor.sprite_height = bounds.y;
    if (actor.reward == 1) {
        float unit;
        if (!random_value(owner, 0, 0, 0, &unit, error))
            return false;
        actor.angle_speed = (unit * 0x1.921fb6p+1f) * 1.5f;
    } else if (actor.reward == 2) {
        if (!random_value(owner, 2, screen.x * 0.1f, screen.x * 0.9f, &actor.position.x,
                          error) ||
            !random_value(owner, 2, -250, -150, &y_speed, error))
            return false;
        actor.velocity.y = ((y_speed * actor.scale) * (screen.y / 320 + 1)) * 0.175f;
        actor.velocity.x = 0;
    }
    if (!yell(&actor, error))
        return false;
    if (owner->actor_count == AB_UI_MENU_BIRD_CAPACITY)
        return ab_fail(error, "Menu bird actor storage exhausted");
    owner->actors[owner->actor_count++] = actor;
    return true;
}

bool ab_ui_menu_birds_update(AbUiMenuBirds *owner, AbUiWidgetContext *context,
                             AbVec2 screen, float seconds, AbError *error) {
    if (!valid(owner, screen, error) || !context || !context->images.bounds ||
        !isfinite(seconds))
        return ab_fail(error, "Invalid menu bird animation context");
    if ((float)owner->actor_count < (float)owner->candidate_count * 0.75f) {
        float choice;
        if (!random_value(owner, 2, 1, 5, &choice, error) ||
            (choice == 1 && !spawn(owner, context, screen, error)))
            return false;
    }
    for (size_t index = owner->actor_count; index; --index) {
        AbUiMenuBirdActor *actor = owner->actors + index - 1;
        if (actor->reward == 2) {
            float sine;
            if (!ab_script_sine(actor->angle_speed, &sine))
                return ab_fail(error, "Menu balloon angle exceeds sine domain");
            actor->angle = sine * 0.15f;
            float phase = actor->angle_speed + seconds * 2;
            float cycle = 0x1.921fb6p+1f * 2;
            actor->angle_speed =
                (float)((double)phase - floor((double)(phase / cycle)) * (double)cycle);
            actor->position.x -= ((actor->angle * (float)actor->layer) * seconds) * 30;
        } else {
            actor->velocity.y += 150 * seconds;
            actor->angle += actor->angle_speed * seconds;
        }
        actor->position.x += actor->velocity.x * seconds;
        actor->position.y += actor->velocity.y * seconds;
        if (!isfinite(actor->position.x) || !isfinite(actor->position.y) ||
            !isfinite(actor->angle) || !isfinite(actor->velocity.y) ||
            !isfinite(actor->angle_speed))
            return ab_fail(error, "Menu bird animation overflow");
        if (actor->position.y > screen.y + (80 * screen.y) / 320 ||
            (actor->reward == 2 && actor->position.y < -actor->sprite_height)) {
            size_t remaining = owner->actor_count - index;
            memmove(actor, actor + 1, remaining * sizeof(*actor));
            --owner->actor_count;
        }
    }
    return true;
}

bool ab_ui_menu_birds_draw(AbUiMenuBirds *owner, AbUiWidgetContext *context,
                           const AbSpriteProvider *sprites, AbVec2 screen,
                           AbError *error) {
    if (!valid(owner, screen, error) || !context || !context->render_state ||
        !context->images.pivot || !sprites || !sprites->sprite)
        return ab_fail(error, "Invalid menu bird rendering context");
    const float factors[] = {0.7f, 0.8f, 0.9f};
    const float offsets[] = {0.2f, 0.125f, 0};
    for (unsigned layer = 3; layer <= 5; ++layer) {
        for (size_t index = 0; index < owner->actor_count; ++index) {
            const AbUiMenuBirdActor *actor = owner->actors + index;
            if (actor->layer != layer)
                continue;
            float scale = (actor->scale * factors[layer - 3]) * (screen.y / 640);
            AbVec2 pivot;
            if (!isfinite(scale) || scale == 0)
                return ab_fail(error, "Invalid menu bird rendering scale");
            if (!context->images.pivot(context->images.context, actor->sprite, &pivot,
                                       error))
                return false;
            AbUiImageRenderState *state = context->render_state;
            state->geometry.translation_x = state->geometry.translation_y = 0;
            state->geometry.scale_x = state->geometry.scale_y = scale;
            state->geometry.angle_radians = actor->angle;
            state->geometry.rotation_origin_x = pivot.x;
            state->geometry.rotation_origin_y = pivot.y;
            AbUiSpriteRender draw = {
                .image = actor->sprite,
                .position = {floorf(actor->position.x / scale),
                             floorf(actor->position.y / scale -
                                    (screen.y * offsets[layer - 3]) / actor->scale)},
                .vertical = AB_ANCHOR_VPIVOT,
                .horizontal = AB_ANCHOR_HPIVOT};
            AbSpriteAtlasBinding ordinary;
            AbResolvedComposite composite;
            bool present =
                sprites->sprite(sprites->context, actor->sprite, &ordinary) ||
                (sprites->composite &&
                 sprites->composite(sprites->context, actor->sprite, &composite));
            if (present && !ab_ui_sprite_render(&draw, state, &context->render,
                                                context->draw_list, error))
                return false;
        }
    }
    return true;
}
