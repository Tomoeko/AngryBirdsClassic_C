#include "ab/classic_highscores.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry fields[4];
} HighscoreEvent;

struct AbClassicHighscores {
    AbClassicHighscoresConfig config;
    HighscoreEvent *notices;
    size_t used;
    bool calling, failed;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(span(name)));
    return value ? *value : (AbDataValue){0};
}

static AbDataValue record(AbClassicHighscores *owner, AbSpan level) {
    const AbDataValue *value =
        ab_data_get(ab_settings_statistics_root(owner->config.storage), text(level));
    return value ? *value : (AbDataValue){0};
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Unsupported highscore record");
    *output = value.table;
    return true;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached highscore comparison");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite highscore comparison");
}

static bool equal(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable singleton = {&entry, 1, 1, NULL};
    return ab_data_get(&singleton, right) != NULL;
}

static bool write(AbClassicHighscores *owner, AbSpan level, const char *name,
                  AbDataValue value, AbError *error) {
    AbSpan path[] = {level, span(name)};
    return ab_settings_statistics_write(owner->config.storage, path, 2, value, error);
}

static bool initialize(AbClassicHighscores *owner, AbSpan level, AbError *error) {
    AbDataValue value = record(owner, level);
    if (value.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(owner->config.storage, &level, 1, 1,
                                                 error) ||
            !write(owner, level, "score", number(0), error))
            return false;
        value = record(owner, level);
    }
    const AbDataTable *row = NULL;
    if (!table(value, &row, error))
        return false;
    if (field(row, "score").kind == AB_DATA_NIL &&
        !write(owner, level, "score", number(0), error))
        return false;
    if (field(row, "legitScore").kind == AB_DATA_NIL &&
        !write(owner, level, "legitScore",
               equal(field(row, "score"), field(row, "powerUpScore"))
                   ? number(0)
                   : field(row, "score"),
               error))
        return false;
    return field(row, "powerUpScore").kind != AB_DATA_NIL ||
           write(owner, level, "powerUpScore",
                 equal(field(row, "score"), field(row, "legitScore"))
                     ? number(0)
                     : field(row, "score"),
                 error);
}

static bool current(AbClassicHighscores *owner, AbSpan level, AbDataValue powerups,
                    AbDataValue *output, AbError *error) {
    AbDataValue value = record(owner, level);
    if (!truthy(value)) {
        *output = number(0);
        return true;
    }
    const AbDataTable *row = NULL;
    if (!table(value, &row, error))
        return false;
    if ((!truthy(field(row, "score")) || !truthy(field(row, "legitScore")) ||
         !truthy(field(row, "powerUpScore"))) &&
        !initialize(owner, level, error))
        return false;
    *output = field(row, truthy(*owner->config.separate_powerups)
                             ? (truthy(powerups) ? "powerUpScore" : "legitScore")
                             : "score");
    return true;
}

static bool service(AbClassicHighscores *owner, AbClassicHighscoreService operation,
                    AbDataValue selection, AbSpan level, float score, float birds,
                    AbDataValue powerups, AbError *error) {
    return owner->config.service
               ? owner->config.service(owner->config.context, operation, selection,
                                       level, score, birds, powerups, error)
               : ab_fail(error, "Missing reached highscore service %u",
                         (unsigned)operation);
}

static bool improve(AbClassicHighscores *owner, AbSpan level, float score, float birds,
                    AbDataValue powerups, AbError *error) {
    AbClassicHighscoresConfig *config = &owner->config;
    if (truthy(*config->started_from_map) &&
        !service(owner, AB_HIGHSCORE_MAP, *config->started_from_map, level, score,
                 birds, powerups, error))
        return false;
    const AbDataTable *row = NULL;
    if (!table(record(owner, level), &row, error))
        return false;
    AbDataValue old = field(row, "score");
    if (truthy(*config->started_sublevel) && truthy(*config->chapter_service) &&
        !service(owner, AB_HIGHSCORE_CHAPTER, *config->started_sublevel, level, score,
                 birds, powerups, error))
        return false;
    float previous;
    if (!numeric(old, &previous, error))
        return false;
    if (previous < score) {
        if (!write(owner, level, "score", number(score), error) ||
            !write(owner, level, "birds", number(birds), error))
            return false;
        if (truthy(*config->native_gamer_service) &&
            !service(owner, AB_HIGHSCORE_RESET_LEADERBOARDS,
                     *config->native_gamer_service, level, score, birds, powerups,
                     error))
            return false;
        if (owner->used == config->notifications)
            return ab_fail(error, "Highscore notification capacity exhausted");
        HighscoreEvent *notice = owner->notices + owner->used++;
        notice->fields[0] =
            (AbDataEntry){text(span("id")), config->new_highscore_event};
        notice->fields[1] = (AbDataEntry){text(span("level")), text(level)};
        notice->fields[2] = (AbDataEntry){text(span("oldHighscore")), old};
        notice->fields[3] = (AbDataEntry){text(span("newHighscore")), number(score)};
        notice->table = (AbDataTable){notice->fields, 4, 4, NULL};
        if (!ab_event_manager_notify(config->events, &notice->table, error))
            return false;
    }
    const char *channel = truthy(powerups) ? "powerUpScore" : "legitScore";
    if (!table(record(owner, level), &row, error) ||
        !numeric(field(row, channel), &previous, error) ||
        (previous < score && !write(owner, level, channel, number(score), error)))
        return false;
    AbDataValue low = field(row, "lowScore");
    if (low.kind != AB_DATA_NIL && !numeric(low, &previous, error))
        return false;
    return (low.kind != AB_DATA_NIL && !(score < previous)) ||
           write(owner, level, "lowScore", number(score), error);
}

bool ab_classic_highscores_open(AbClassicHighscoresConfig config,
                                AbClassicHighscores **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing highscore owner output");
    *output = NULL;
    if (!config.storage || !config.events || !config.separate_powerups ||
        !config.started_from_map || !config.started_sublevel ||
        !config.chapter_service || !config.native_gamer_service ||
        config.new_highscore_event.kind == AB_DATA_NIL || !config.notifications ||
        config.storage_bytes < sizeof(AbClassicHighscores) ||
        config.notifications > (config.storage_bytes - sizeof(AbClassicHighscores)) /
                                   sizeof(HighscoreEvent))
        return ab_fail(error, "Invalid actual highscore bindings or budget");
    AbClassicHighscores *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate highscore owner");
    owner->config = config;
    owner->notices = calloc(config.notifications, sizeof(*owner->notices));
    if (!owner->notices) {
        free(owner);
        return ab_fail(error, "Cannot allocate retained highscore notifications");
    }
    *output = owner;
    return true;
}

void ab_classic_highscores_close(AbClassicHighscores *owner) {
    if (!owner)
        return;
    free(owner->notices);
    free(owner);
}

static bool begin(AbClassicHighscores *owner, AbSpan level, AbError *error) {
    if (!owner || owner->calling || owner->failed || !level.data || !level.size)
        return ab_fail(error, "Invalid, failed or reentrant highscore call");
    owner->calling = true;
    return true;
}

static bool finish(AbClassicHighscores *owner, bool okay) {
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}

bool ab_classic_highscores_initialize(AbClassicHighscores *owner, AbSpan level,
                                      AbError *error) {
    if (!begin(owner, level, error))
        return false;
    return finish(owner, initialize(owner, level, error));
}

bool ab_classic_highscores_current(AbClassicHighscores *owner, AbSpan level,
                                   AbDataValue powerups, AbDataValue *output,
                                   AbError *error) {
    if (!output || !begin(owner, level, error))
        return output ? false : ab_fail(error, "Missing current highscore result");
    return finish(owner, current(owner, level, powerups, output, error));
}

bool ab_classic_highscores_improve(AbClassicHighscores *owner, AbSpan level,
                                   float score, float birds, AbDataValue powerups,
                                   AbError *error) {
    if (!begin(owner, level, error))
        return false;
    bool okay = (isfinite(score) && isfinite(birds)) ||
                ab_fail(error, "Nonfinite improved highscore arguments");
    return finish(owner, okay && improve(owner, level, score, birds, powerups, error));
}
