#include "ab/classic_level_ending.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry entries[4];
    AbDataTable table;
} EndingEvent;

struct AbClassicLevelEnding {
    AbClassicLevelEndingConfig config;
    AbClassicLevelView level;
    EndingEvent *events;
    size_t used, bytes;
    bool updating, failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool literal_true(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && value.bits;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *found = ab_data_get(table, text(name));
    return found ? *found : (AbDataValue){0};
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported level-ending numeric value");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite level-ending number");
}

static bool subtract(float *timer, float seconds, AbError *error) {
    float next = *timer - seconds;
    if (!isfinite(*timer) || !isfinite(seconds) || !isfinite(next))
        return ab_fail(error, "Unsupported level-ending timer arithmetic");
    *timer = next;
    return true;
}

static bool before_ending(AbClassicLevelEndingConfig *config, float seconds,
                          AbError *error) {
    if (!config->state->before_script_called) {
        config->state->before_script_called = true;
        if (config->before_ending && !config->before_ending(config->context, error))
            return false;
    }
    return subtract(&config->finish->complete.ending_timer, seconds, error);
}

static bool bird_effects(AbClassicLevelEnding *ending, size_t object, AbError *error) {
    AbClassicLevelEndingConfig *config = &ending->config;
    AbDataValue selected;
    uint32_t handle;
    if (!ab_audio_bank_name_value(config->audio, *config->bird_sound, &selected,
                                  error) ||
        !ab_audio_bank_play(config->audio,
                            (AbAudioPlay){.name = selected,
                                          .volume = number(1),
                                          .loop = {.kind = AB_DATA_BOOLEAN},
                                          .channel = number(0)},
                            &handle, error))
        return false;
    AbDataTable *physical = &ending->level.properties->objects[object].physical;
    AbBirdCollisionFloatingScore score = {.score = config->state->score_increment,
                                          .lifetime = 0.9f};
    if (!numeric(field(physical, "x"), &score.position.x, error) ||
        !numeric(field(physical, "y"), &score.position.y, error))
        return false;
    AbDataValue definition;
    AbSpan name = ending->level.bodies->objects[object].name;
    if (!ab_object_definition_lookup(&ending->level.properties->lookup, name,
                                     &definition, error))
        return false;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing unused-bird score definition");
    score.sprite = field(definition.table, "spriteScore");
    score.max_scale = config->state->floating_scale * 1.0f;
    return ab_floating_scores_add(config->floating, &score, error);
}

static bool award_bird(AbClassicLevelEnding *ending, size_t object, AbError *error) {
    AbClassicLevelEndingConfig *config = &ending->config;
    AbLevelCompleteState *finish = &config->finish->complete;
    float next = finish->remaining_bird_index + 1.0f;
    if (!isfinite(finish->remaining_bird_index) || !isfinite(next))
        return ab_fail(error, "Unsupported ending bird index");
    finish->remaining_bird_index = next;
    if (!literal_true(*config->eagle_launched)) {
        float *total = ab_classic_scores_view(config->scores).birds;
        float increment = config->state->score_increment;
        next = *total + increment;
        if (!isfinite(*total) || !isfinite(increment) || !isfinite(next))
            return ab_fail(error, "Unsupported unused-bird score arithmetic");
        *total = next;
        if (config->bonus->count >= config->bonus->capacity ||
            (config->bonus->capacity && !config->bonus->entries))
            return ab_fail(error, "Unused-bird score append capacity exhausted");
        config->bonus->entries[config->bonus->count++] = increment;
    }
    return bird_effects(ending, object, error);
}

static bool update_score(AbClassicLevelEndingConfig *config, AbError *error) {
    float total = 0;
    bool okay = ab_classic_scores_update(config->scores, &total, error);
    AbClassicGameGlobals *globals = ab_classic_shots_globals(config->shots);
    AbError ignored = {0};
    bool published =
        ab_classic_game_globals_publish_score(globals, total, okay ? error : &ignored);
    return okay && published && ab_classic_shots_refresh_globals(config->shots, error);
}

static bool completed_update(AbClassicLevelEnding *ending, float seconds,
                             AbError *error) {
    AbClassicLevelEndingConfig *config = &ending->config;
    AbLevelCompleteState *finish = &config->finish->complete;
    config->state->eagle_darkness = (AbDataValue){0};
    if (config->eagle_darkness)
        *config->eagle_darkness = (AbDataValue){0};
    if (!subtract(&finish->bonus_timer, seconds, error))
        return false;
    if (finish->bonus_timer >= 0)
        return true;
    size_t object;
    if (!ab_classic_sling_next_bird(config->sling, finish->remaining_bird_index,
                                    &object, error))
        return false;
    if (object != SIZE_MAX) {
        if (!award_bird(ending, object, error))
            return false;
        finish->bonus_timer = 0.5f;
    } else if (!before_ending(config, seconds, error)) {
        return false;
    }
    if (!update_score(config, error))
        return false;
    if (!literal_true(config->state->cheats_enabled) ||
        !literal_true(config->state->quad_click))
        return true;
    if (!config->stars)
        return ab_fail(error, "Missing actual level-ending star table");
    const AbDataValue *entry = ab_data_get(config->stars, config->state->level_name);
    if (!entry || entry->kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual ending star record");
    AbClassicGameGlobals *globals = ab_classic_shots_globals(config->shots);
    globals->score = field(entry->table, "goldScore");
    if (literal_true(*config->eagle_launched))
        globals->score = config->state->eagle_score;
    return ab_classic_shots_refresh_globals(config->shots, error);
}

static bool queue_result(AbClassicLevelEnding *ending, AbError *error) {
    AbClassicLevelEndingConfig *config = &ending->config;
    bool completed = config->finish->complete.completed;
    if (!completed &&
        !ab_classic_character_services_win(config->conditions, &completed, error))
        return false;
    if (ending->used == config->notifications)
        return ab_fail(error, "Level-ended event capacity exhausted");
    EndingEvent *event = ending->events + ending->used++;
    event->entries[0] = (AbDataEntry){text("id"), text("EID_LEVEL_ENDED")};
    event->entries[1] = (AbDataEntry){text("level"), config->state->level_name};
    event->entries[2] =
        (AbDataEntry){text("score"), ab_classic_shots_globals(config->shots)->score};
    event->entries[3] = (AbDataEntry){text("levelComplete"),
                                      {.kind = AB_DATA_BOOLEAN, .bits = completed}};
    event->table = (AbDataTable){event->entries, 4, 4, NULL};
    return ab_event_manager_queue(config->events, &event->table, error);
}

static bool skip_ending(AbClassicLevelEnding *ending, AbError *error) {
    AbClassicLevelEndingConfig *config = &ending->config;
    AbLevelCompleteState *finish = &config->finish->complete;
    AbShotState *shot = ab_classic_shots_state(config->shots);
    config->state->skip_ending = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    finish->completed = true;
    finish->remaining_bird_index =
        ab_classic_sling_view(config->sling).current_bird_index;
    bool spent = !shot->current_bird_name;
    if (!spent) {
        AbDataValue current;
        if (!ab_object_world_lookup(&ending->level.properties->lookup,
                                    text(shot->current_bird_name), &current, error))
            return false;
        if (current.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing current bird during level-end shortcut");
        spent = literal_true(field(current.table, "shot"));
    }
    if (spent && shot->birds_shot > 0)
        finish->remaining_bird_index += 1;
    float *birds = ab_classic_scores_view(config->scores).birds;
    *birds = 0;
    for (;;) {
        size_t object;
        if (!ab_classic_sling_next_bird(config->sling, finish->remaining_bird_index,
                                        &object, error))
            return false;
        if (object == SIZE_MAX)
            break;
        finish->remaining_bird_index += 1;
        if (!truthy(*config->eagle_launched)) {
            float increment = config->state->score_increment;
            float next = *birds + increment;
            if (!isfinite(increment) || !isfinite(next))
                return ab_fail(error, "Nonfinite fast-forward bird bonus");
            *birds = next;
            if (config->bonus->count == config->bonus->capacity)
                return ab_fail(error, "Fast-forward bird bonus storage exhausted");
            config->bonus->entries[config->bonus->count++] = increment;
        }
    }
    return update_score(config, error) && queue_result(ending, error);
}

bool ab_classic_level_ending_skip(AbClassicLevelEnding *ending, AbError *error) {
    if (!ending || ending->failed || ending->updating)
        return ab_fail(error, "Invalid or reentrant level-end shortcut");
    ending->updating = true;
    bool okay = ab_classic_shots_refresh_globals(ending->config.shots, error) &&
                skip_ending(ending, error);
    ending->updating = false;
    ending->failed = !okay;
    return okay;
}

static bool ending_delay(AbClassicLevelEnding *ending, float seconds, AbError *error) {
    AbClassicLevelEndingConfig *config = &ending->config;
    AbClassicLevelEndingState *state = config->state;
    AbLevelCompleteState *finish = &config->finish->complete;
    if (state->do_not_wait_delay.kind != AB_DATA_NIL) {
        float delay;
        if (!numeric(state->do_not_wait_delay, &delay, error) ||
            !subtract(&delay, seconds, error))
            return false;
        state->do_not_wait_delay = number(delay);
        if (delay <= 0) {
            state->do_not_wait = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
            state->do_not_wait_delay = (AbDataValue){0};
        }
    }
    if (literal_true(state->do_not_wait) && !state->delay_set) {
        state->delay_set = true;
        finish->ending_timer = state->default_delay;
    }
    if (truthy(*config->conditions->config.moving_objects) &&
        !literal_true(state->do_not_wait) && finish->ending_timer < 1) {
        if (!isfinite(state->extensions))
            return ab_fail(error, "Unsupported ending delay extensions");
        if (state->extensions < 3) {
            finish->ending_timer = 2;
            state->extensions += 1;
        }
    }
    if (!isfinite(finish->ending_timer))
        return ab_fail(error, "Unsupported ending timer");
    if (finish->ending_timer <= 0) {
        ab_classic_shots_globals(config->shots)->level_timeout =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        return queue_result(ending, error);
    }
    return true;
}

bool ab_classic_level_ending_open(AbClassicLevelEndingConfig config,
                                  AbClassicLevelEnding **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned level-ending output");
    *output = NULL;
    AbClassicLevelView level =
        ab_classic_level_view(ab_classic_shots_level(config.shots));
    if (!level.bodies || !level.properties || !ab_classic_shots_globals(config.shots) ||
        !config.sling || !ab_classic_scores_view(config.scores).birds ||
        !config.floating || !config.audio || !config.events || !config.conditions ||
        !config.conditions->initialized || !config.finish || !config.state ||
        !config.bonus || !config.eagle_launched || !config.bird_sound ||
        config.bonus->count > config.bonus->capacity ||
        (config.bonus->capacity && !config.bonus->entries) ||
        config.storage_bytes < sizeof(AbClassicLevelEnding) ||
        config.notifications >
            (config.storage_bytes - sizeof(AbClassicLevelEnding)) / sizeof(EndingEvent))
        return ab_fail(error, "Invalid owned level-ending bindings");
    AbClassicGameGlobals *globals = ab_classic_shots_globals(config.shots);
    if (config.conditions->config.level != ab_classic_shots_level(config.shots) ||
        config.conditions->config.level_timeout != &globals->level_timeout ||
        config.conditions->config.do_not_wait_for_moving_objects !=
            &config.state->do_not_wait ||
        config.conditions->config.eagle_launched != config.eagle_launched)
        return ab_fail(error, "Mismatched canonical level-ending condition bindings");
    AbClassicLevelEnding *ending = calloc(1, sizeof(*ending));
    if (!ending)
        return ab_fail(error, "Cannot allocate owned level ending");
    ending->config = config;
    ending->level = level;
    ending->bytes = sizeof(*ending) + config.notifications * sizeof(EndingEvent);
    if (config.notifications)
        ending->events = calloc(config.notifications, sizeof(*ending->events));
    if (config.notifications && !ending->events) {
        ab_classic_level_ending_close(ending);
        return ab_fail(error, "Cannot allocate retained level-ending events");
    }
    *output = ending;
    return true;
}

void ab_classic_level_ending_close(AbClassicLevelEnding *ending) {
    if (ending) {
        free(ending->events);
        free(ending);
    }
}

size_t ab_classic_level_ending_bytes(const AbClassicLevelEnding *ending) {
    return ending ? ending->bytes : 0;
}

bool ab_classic_level_ending_update(AbClassicLevelEnding *ending, float seconds,
                                    AbError *error) {
    if (!ending || ending->updating || ending->failed || ending->level.world->failed ||
        ending->level.world->locked)
        return ab_fail(error, "Invalid, failed or reentrant owned level ending");
    ending->updating = true;
    AbClassicLevelEndingConfig *config = &ending->config;
    bool okay = true;
    if (!truthy(config->state->skip_ending)) {
        okay = config->finish->complete.completed
                   ? completed_update(ending, seconds, error)
                   : subtract(&config->finish->complete.ending_timer, seconds, error);
        okay = okay && ending_delay(ending, seconds, error);
    }
    ending->updating = false;
    ending->failed = !okay;
    return okay;
}
