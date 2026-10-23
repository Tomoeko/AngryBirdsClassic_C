#include "ab/classic_powerup_manager.h"

#include <string.h>

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) && !memcmp(value.data, name, value.size);
}

static bool healthy(AbClassicPowerupManager *manager, size_t index, AbError *error) {
    return (manager && manager->initialized && !manager->failed &&
            index < AB_CLASSIC_POWERUPS) ||
           ab_fail(error, "Invalid or stopped powerup manager");
}

static bool contains(AbClassicPowerupUpdate **items, AbClassicPowerupUpdate *item) {
    if (!item)
        return false;
    for (size_t index = 0; index < AB_CLASSIC_POWERUPS && items[index]; ++index)
        if (items[index] == item)
            return true;
    return false;
}

bool ab_classic_powerup_manager_in_use(AbClassicPowerupManager *manager, size_t powerup,
                                       bool *output, AbError *error) {
    if (!output || !healthy(manager, powerup, error))
        return false;
    AbClassicPowerupUpdate *item = manager->config.instances[powerup];
    *output = contains(manager->active, item) || contains(manager->passive, item);
    return true;
}

bool ab_classic_powerup_manager_initialize(AbClassicPowerupManagerConfig config,
                                           AbClassicPowerupManager *output,
                                           AbError *error) {
    if (!output || ab_powerup_catalog_count(config.catalog) != AB_CLASSIC_POWERUPS ||
        !config.settings || !config.events || !config.uses || !config.sling ||
        !config.flying || !config.specialty_available || !config.level_name ||
        !config.active || !config.passive)
        return ab_fail(error, "Missing actual powerup manager registration owners");
    *output = (AbClassicPowerupManager){.config = config, .initialized = true};
    for (size_t index = 0; index < AB_CLASSIC_POWERUPS; ++index) {
        const AbPowerupMetadata *row = ab_powerup_catalog_at(config.catalog, index);
        output->infinite[index] = config.infinite || truthy(row->infinite);
        output->enabled[index] = true;
    }
    *config.active = (AbClassicPowerupUpdates){output->active, AB_CLASSIC_POWERUPS + 1};
    *config.passive =
        (AbClassicPowerupUpdates){output->passive, AB_CLASSIC_POWERUPS + 1};
    return true;
}

bool ab_classic_powerup_manager_can_activate(AbClassicPowerupManager *manager,
                                             size_t powerup, bool *output,
                                             AbError *error) {
    if (!output || !healthy(manager, powerup, error))
        return false;
    *output = false;
    AbClassicPowerupManagerConfig *config = &manager->config;
    if (contains(manager->active, config->instances[powerup]))
        return true;
    const AbPowerupMetadata *row = ab_powerup_catalog_at(config->catalog, powerup);
    char name[64];
    if (row->name.size >= sizeof(name))
        return ab_fail(error, "Powerup identifier exceeds activation domain");
    memcpy(name, row->name.data, row->name.size);
    name[row->name.size] = 0;
    bool used;
    if (!ab_powerup_uses_contains(config->uses, name, &used, error))
        return false;
    if (used && !named(row->name, "powerpotion") && !named(row->name, "birdquake") &&
        !named(row->name, "shockwave"))
        return true;
    size_t next;
    AbClassicSlingView sling = ab_classic_sling_view(config->sling);
    if (!ab_classic_sling_next_bird(config->sling, sling.current_bird_index, &next,
                                    error))
        return false;
    bool allow = truthy(field(row->definition, "allowActivationWhenNoBirdsLeft"));
    if (next == SIZE_MAX && !allow)
        return true;
    AbClassicLevelView level = ab_classic_level_view(
        ab_classic_shots_level(ab_classic_sling_shots(config->sling)));
    const AbDataTable *bird =
        next == SIZE_MAX ? NULL : &level.properties->objects[next].physical;
    if (named(row->name, "powerpotion") && truthy(field(bird, "usedPowerPotion")))
        return true;
    if (named(row->name, "birdquake") || named(row->name, "double")) {
        *output = true;
        return true;
    }
    if (named(row->name, "shockwave")) {
        for (size_t index = 0; index < level.bird_count; ++index) {
            size_t object = level.bird_order[index];
            const AbDataTable *candidate = &level.properties->objects[object].physical;
            if (truthy(field(candidate, "bonusShockwaveBird")) &&
                !truthy(field(candidate, "shot")))
                return true;
        }
    }
    if ((named(row->name, "shockwave") || named(row->name, "redsmightyfeathers")) &&
        truthy(*config->specialty_available) && config->flying->kind == AB_DATA_TABLE &&
        truthy(field(config->flying->table, "shot")))
        return true;
    *output = (bird && !truthy(field(bird, "shot"))) || allow;
    return true;
}

static AbClassicPowerupNotice *notice(AbClassicPowerupManager *manager, const char *id,
                                      AbError *error) {
    if (manager->notice_count == AB_CLASSIC_POWERUP_NOTICES) {
        ab_fail(error, "Powerup notification storage exhausted");
        return NULL;
    }
    AbClassicPowerupNotice *notice = manager->notices + manager->notice_count++;
    notice->fields[0] = (AbDataEntry){text("id"), text(id)};
    notice->table = (AbDataTable){notice->fields, 1, 6, NULL};
    return notice;
}

static bool activate(AbClassicPowerupManager *manager, size_t powerup, AbDataValue from,
                     bool *output, AbError *error) {
    AbClassicPowerupManagerConfig *config = &manager->config;
    const AbPowerupMetadata *row = ab_powerup_catalog_at(config->catalog, powerup);
    AbDataValue name = {.kind = AB_DATA_STRING, .string = row->name};
    if (!config->activate || !config->instances[powerup])
        return ab_fail(error, "Powerup requires its recovered effect: %.*s",
                       (int)row->name.size, row->name.data);
    size_t count = 0;
    while (count < AB_CLASSIC_POWERUPS && manager->active[count])
        ++count;
    if (count == AB_CLASSIC_POWERUPS)
        return ab_fail(error, "Powerup active list exhausted");
    manager->active[count] = config->instances[powerup];
    AbClassicPowerupNotice *will = notice(manager, "EID_POWERUP_WILL_BE_USED", error);
    if (!will)
        return false;
    will->fields[will->table.count++] = (AbDataEntry){text("type"), name};
    if (!ab_event_manager_notify(config->events, &will->table, error) ||
        (!manager->infinite[powerup] &&
         !ab_settings_inventory_consume(config->settings, row->name, error)))
        return false;
    char identifier[64];
    memcpy(identifier, row->name.data, row->name.size);
    identifier[row->name.size] = 0;
    if (!ab_powerup_uses_mark(config->uses, identifier, error))
        return false;
    AbClassicPowerupNotice *used = notice(manager, "EID_POWERUP_WAS_USED", error);
    if (!used)
        return false;
    used->fields[1] = (AbDataEntry){text("type"), name};
    used->fields[2] = (AbDataEntry){text("total_used"),
                                    number((float)ab_powerup_uses_count(config->uses))};
    used->fields[3] = (AbDataEntry){text("level"), *config->level_name};
    used->fields[4] = (AbDataEntry){text("from"), from};
    used->fields[5] =
        (AbDataEntry){text("consumed"),
                      {.kind = AB_DATA_BOOLEAN, .bits = !manager->infinite[powerup]}};
    used->table.count = 6;
    bool result;
    if (!ab_event_manager_notify(config->events, &used->table, error) ||
        !config->activate(config->context, powerup, &result, error))
        return false;
    AbClassicPowerupNotice *started =
        notice(manager, "EID_POWERUP_ACTIVATION_STARTED", error);
    if (!started || !ab_event_manager_notify(config->events, &started->table, error))
        return false;
    *output = !result;
    return true;
}

bool ab_classic_powerup_manager_activate(AbClassicPowerupManager *manager,
                                         size_t powerup, AbDataValue from, bool *output,
                                         AbError *error) {
    if (!output || !healthy(manager, powerup, error) || manager->calling)
        return ab_fail(error, "Invalid or reentrant powerup activation");
    bool can_activate;
    if (!ab_classic_powerup_manager_can_activate(manager, powerup, &can_activate,
                                                 error))
        return false;
    if (!can_activate) {
        *output = false;
        return true;
    }
    manager->calling = true;
    bool okay = activate(manager, powerup, from, output, error);
    manager->calling = false;
    manager->failed = !okay;
    return okay;
}

bool ab_classic_powerup_manager_deactivate(AbClassicPowerupManager *manager,
                                           size_t powerup, bool passive,
                                           AbError *error) {
    if (!healthy(manager, powerup, error))
        return false;
    AbClassicPowerupUpdate *item = manager->config.instances[powerup];
    for (size_t index = 0; index < AB_CLASSIC_POWERUPS && manager->active[index];) {
        if (manager->active[index] != item) {
            ++index;
            continue;
        }
        if (passive) {
            size_t count = 0;
            while (count < AB_CLASSIC_POWERUPS && manager->passive[count])
                ++count;
            if (count == AB_CLASSIC_POWERUPS)
                return ab_fail(error, "Powerup passive list exhausted");
            manager->passive[count] = item;
        } else if (manager->config.reset[powerup] &&
                   !manager->config.reset[powerup](item->context, error))
            return false;
        memmove(manager->active + index, manager->active + index + 1,
                (AB_CLASSIC_POWERUPS - index) * sizeof(*manager->active));
        AbClassicPowerupNotice *done =
            notice(manager, "EID_POWERUP_ACTIVATION_DONE", error);
        return done &&
               ab_event_manager_notify(manager->config.events, &done->table, error);
    }
    return true;
}
