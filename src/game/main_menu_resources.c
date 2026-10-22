#include "ab/main_menu_resources.h"

#include <string.h>

typedef struct {
    const AbMainMenuResources *resources;
    const AbSpriteProvider *provider;
} ResourceViews;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool table_valid(const AbDataTable *table) {
    if (!table || table->count > table->capacity || (table->count && !table->entries)) {
        return false;
    }
    for (size_t index = 0; index < table->count; ++index) {
        const AbDataValue *key = &table->entries[index].key;
        if (key->kind != AB_DATA_STRING || (key->string.size && !key->string.data)) {
            return false;
        }
    }
    return true;
}

static const AbDataValue *property(const AbDataTable *table, AbSpan name) {
    return ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = name});
}

static bool ui_value(const AbDataValue *source, AbUiPointerValue *output) {
    AbUiPointerValue value = {0};
    if (source) {
        switch (source->kind) {
            case AB_DATA_NIL:
                break;
            case AB_DATA_BOOLEAN:
                value.kind = AB_UI_POINTER_BOOLEAN;
                value.boolean = source->bits != 0;
                break;
            case AB_DATA_NUMBER:
                value.kind = AB_UI_POINTER_NUMBER;
                value.number_bits = source->bits;
                break;
            case AB_DATA_STRING:
                value.kind = AB_UI_POINTER_TEXT;
                value.text = source->string;
                break;
            case AB_DATA_TABLE:
                if (!source->table) {
                    return false;
                }
                value.kind = AB_UI_POINTER_OBJECT;
                value.object = (uintptr_t)source->table;
                break;
            default:
                return false;
        }
    }
    if (!ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    *output = value;
    return true;
}

static bool lookup(void *context, AbUiPointerValue name, bool *output, AbError *error) {
    const ResourceViews *views = context;
    const AbMainMenuResources *resources = views->resources;
    AbUiPointerValue record = {0};
    if (name.kind == AB_UI_POINTER_TEXT &&
        !ui_value(property(resources->themes, name.text), &record)) {
        return ab_fail(error, "Invalid main-menu resource theme record");
    }
    *output = ab_ui_pointer_value_truthy(&record);
    return true;
}

static bool settings(void *context, AbMainMenuSetting operation,
                     AbUiPointerValue *output, AbError *error) {
    const ResourceViews *views = context;
    const AbMainMenuResources *resources = views->resources;
    if (operation == AB_MAIN_MENU_SET_ZOOM) {
        return ab_main_menu_settings_set_zoom(resources->settings,
                                              (AbUiPointerValue){0});
    }
    if (operation == AB_MAIN_MENU_GET_ZOOM) {
        return ab_main_menu_settings_zoom(resources->settings, output);
    }
    if (operation != AB_MAIN_MENU_GET_THEME) {
        return ab_fail(error, "Unsupported main-menu setting query");
    }
    const AbDataValue *config = property(resources->themes, text("settings"));
    if (!config || config->kind != AB_DATA_TABLE || !table_valid(config->table)) {
        return ab_fail(error, "Missing main-menu theme settings table");
    }
    AbUiPointerValue fallback;
    if (!ui_value(property(config->table, text("defaultTheme")), &fallback)) {
        return ab_fail(error, "Invalid main-menu resource default theme");
    }
    return ab_main_menu_settings_theme(resources->settings, fallback, lookup, context,
                                       output, error);
}

static bool sprite(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    const ResourceViews *views = context;
    const AbMainMenuResources *resources = views->resources;
    if (views->provider) {
        const AbSpriteProvider *provider = views->provider;
        AbSpriteAtlasBinding sprite;
        AbResolvedComposite composite;
        if (provider->sprite(provider->context, image, &sprite)) {
            if (!sprite.sprite.region)
                return ab_fail(error, "Invalid menu provider sprite metadata");
            *output =
                (AbVec2){sprite.sprite.region->width, sprite.sprite.region->height};
        } else if (provider->composite &&
                   provider->composite(provider->context, image, &composite)) {
            *output =
                (AbVec2){(float)composite.bounds.width, (float)composite.bounds.height};
        } else {
            *output = (AbVec2){0};
        }
        return true;
    }
    AbSpriteBinding binding;
    if (!ab_sprite_set_find(resources->sprites, image, &binding)) {
        return ab_fail(error, "Main-menu bounds sprite is missing from selected SPRT");
    }
    *output = (AbVec2){(float)binding.region->width, (float)binding.region->height};
    return true;
}

static bool field(void *context, AbSpan theme, AbMainMenuThemeField operation,
                  AbUiPointerValue *output, AbError *error) {
    const ResourceViews *views = context;
    const AbMainMenuResources *resources = views->resources;
    const AbDataValue *record = property(resources->themes, theme);
    if (!record || record->kind != AB_DATA_TABLE || !table_valid(record->table)) {
        return ab_fail(error, "Missing main-menu theme fields");
    }
    const char *fields[] = {"menuTopOffset", "menuWater", "waterLevel"};
    if (operation > AB_MAIN_MENU_WATER_LEVEL ||
        !ui_value(property(record->table, text(fields[operation])), output)) {
        return ab_fail(error, "Invalid main-menu resource field");
    }
    return true;
}

static bool call(void *context, AbMainMenuThemeCall operation, AbSpan theme,
                 const float *numbers, size_t count, AbError *error) {
    const ResourceViews *views = context;
    const AbMainMenuResources *resources = views->resources;
    return resources->call(resources->context, operation, theme, numbers, count, error);
}

static bool layout_resources(AbMainMenuThemeState *state,
                             const AbMainMenuResources *resources,
                             const AbSpriteProvider *provider, float *camera_top,
                             AbError *error) {
    if (!resources || (!resources->sprites && !provider) ||
        (provider && !provider->sprite) || !resources->settings || !resources->call ||
        !table_valid(resources->themes)) {
        return ab_fail(error, "Invalid main-menu resource views");
    }
    ResourceViews views = {resources, provider};
    AbMainMenuThemeBoundaries boundaries = {
        &views, resources->screen_height, resources->scroll, settings, sprite, field,
        call};
    return ab_main_menu_theme_layout(state, &boundaries, camera_top, error);
}

bool ab_main_menu_resources_layout(AbMainMenuThemeState *state,
                                   const AbMainMenuResources *resources,
                                   float *camera_top, AbError *error) {
    return layout_resources(state, resources, NULL, camera_top, error);
}

bool ab_main_menu_resources_layout_provider(AbMainMenuThemeState *state,
                                            const AbMainMenuResources *resources,
                                            const AbSpriteProvider *provider,
                                            float *camera_top, AbError *error) {
    if (!provider)
        return ab_fail(error, "Missing menu resource provider");
    return layout_resources(state, resources, provider, camera_top, error);
}
