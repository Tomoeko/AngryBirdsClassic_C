#include "ab/theme_layers.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

struct AbThemeLayers {
    AbThemeLayerBinding *bindings;
    uintptr_t *handles;
    size_t count, background_count, foreground_count, bytes;
    AbThemeColors colors;
};

static bool table_valid(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static bool same_text(AbSpan text, const char *literal) {
    size_t length = strlen(literal);
    return text.size == length && (!length || memcmp(text.data, literal, length) == 0);
}

static const AbDataValue *property(const AbDataTable *table, const char *name) {
    return ab_data_get(table,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = {(const uint8_t *)name, strlen(name)}});
}

static const AbDataValue *tuple(const AbDataTable *table, size_t index) {
    float value = (float)index;
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits});
}

static const AbDataTable *layer_list(const AbDataTable *theme, const char *name) {
    const AbDataValue *value = property(theme, name);
    return value && value->kind == AB_DATA_TABLE && table_valid(value->table)
               ? value->table
               : NULL;
}

static bool dense_count(const AbDataTable *list, size_t limit, size_t *output,
                        AbError *error) {
    size_t count = 0;
    for (size_t index = 1; index <= UINT32_C(16777216); ++index) {
        const AbDataValue *value = tuple(list, index);
        if (!value || value->kind != AB_DATA_TABLE) {
            *output = count;
            return true;
        }
        if (!table_valid(value->table) || count == limit) {
            return ab_fail(error, "Theme layer list exceeds schema/allocation bounds");
        }
        ++count;
    }
    return ab_fail(error, "Theme layer integer indices exceed exact float domain");
}

static bool base_type(const AbDataTable *table, AbError *error) {
    const AbDataValue *type = property(table, "type");
    if (!type || type->kind != AB_DATA_STRING) {
        return true;
    }
    if (type->string.size && !type->string.data) {
        return ab_fail(error, "Invalid theme layer type string");
    }
    const char *const special[] = {"rect", "screen", "menu_particles"};
    for (size_t index = 0; index < 3; ++index) {
        if (same_text(type->string, special[index])) {
            return ab_fail(error, "Theme layer subtype construction requires recovery");
        }
    }
    return true;
}

typedef struct {
    const AbSpriteSet *sprites;
    const AbCompositeResources *composites;
    const AbSpriteProvider *provider;
} LayerSources;

static bool border_number(const AbDataTable *table, const char *name, float fallback,
                          float *output, AbError *error) {
    const AbDataValue *value = property(table, name);
    if (!value || value->kind != AB_DATA_NUMBER) {
        if (value && value->kind == AB_DATA_STRING)
            return ab_fail(error,
                           "Numeric string screen-border fields are unsupported");
        *output = fallback;
        return true;
    }
    memcpy(output, &value->bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite screen-border field");
}

static bool bind_border(const AbDataTable *table, const LayerSources *sources,
                        AbThemeLayerBinding *output, AbError *error) {
    const AbDataValue *sprite = property(table, "sprite");
    if (sprite && sprite->kind == AB_DATA_NUMBER)
        return ab_fail(error, "Numeric screen-border sprite coercion is unsupported");
    AbSpan name = {(const uint8_t *)"", 0};
    if (sprite && sprite->kind == AB_DATA_STRING)
        name = sprite->string;
    if (name.size && !name.data)
        return ab_fail(error, "Invalid screen-border sprite name");
    output->kind = AB_THEME_SPRITE_BORDER;
    output->definition.sprite = name;
    output->definition.base.scale = 1;
    if (!border_number(table, "borderSize", 0.15f, &output->border_size, error) ||
        !border_number(table, "alpha", 1, &output->border_alpha, error))
        return false;
    if (sources->provider) {
        AbSpriteAtlasBinding binding;
        if (sources->provider->sprite(sources->provider->context, name, &binding))
            output->ordinary = binding.sprite;
        else if (sources->provider->composite &&
                 sources->provider->composite(sources->provider->context, name,
                                              &output->composite))
            return ab_fail(error, "Composite screen-border drawing is unsupported");
    } else if (!ab_sprite_set_find(sources->sprites, name, &output->ordinary) &&
               ab_composite_resources_find(sources->composites, name,
                                           &output->composite)) {
        return ab_fail(error, "Composite screen-border drawing is unsupported");
    }
    return true;
}

static bool bind_layer(const AbDataTable *table, const LayerSources *sources,
                       AbThemeLayerBinding *output, AbError *error) {
    const AbDataValue *type = property(table, "type");
    if (type && type->kind == AB_DATA_STRING && type->string.size && !type->string.data)
        return ab_fail(error, "Invalid theme layer type string");
    if (type && type->kind == AB_DATA_STRING && same_text(type->string, "border"))
        return bind_border(table, sources, output, error);
    const AbDataValue *name = tuple(table, 2);
    if (!base_type(table, error)) {
        return false;
    }
    if (!name || name->kind != AB_DATA_STRING ||
        (name->string.size && !name->string.data)) {
        return ab_fail(error, "Static theme layer requires string sprite identifier");
    }
    AbSpriteAtlasBinding binding;
    bool ordinary =
        sources->provider
            ? sources->provider->sprite(sources->provider->context, name->string,
                                        &binding)
            : ab_sprite_set_find(sources->sprites, name->string, &output->ordinary);
    if (ordinary) {
        if (sources->provider)
            output->ordinary = binding.sprite;
        if (!output->ordinary.region)
            return ab_fail(error, "Missing provider theme sprite metadata");
        output->kind = AB_THEME_SPRITE_ORDINARY;
        return ab_theme_layer_definition(table, output->ordinary.region,
                                         &output->definition, error);
    }
    AbThemeSpriteMetrics metrics = {0};
    bool composite =
        sources->provider
            ? sources->provider->composite &&
                  sources->provider->composite(sources->provider->context, name->string,
                                               &output->composite)
            : ab_composite_resources_find(sources->composites, name->string,
                                          &output->composite);
    if (composite) {
        output->kind = AB_THEME_SPRITE_COMPOSITE;
        const AbCompositeBounds *bounds = &output->composite.bounds;
        metrics = (AbThemeSpriteMetrics){true, bounds->width, bounds->height,
                                         bounds->pivot_x, bounds->pivot_y};
    }
    return ab_theme_layer_definition_metrics(table, &metrics, &output->definition,
                                             error);
}

static bool open_layers(const AbDataTable *theme, LayerSources sources,
                        size_t allocation_limit, AbThemeLayers **output,
                        AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing owned theme layer output");
    }
    *output = NULL;
    if (!table_valid(theme) || (!sources.sprites && !sources.provider) ||
        (sources.provider && !sources.provider->sprite) ||
        allocation_limit < sizeof(AbThemeLayers)) {
        return ab_fail(error, "Invalid owned theme layer input/budget");
    }
    AbThemeColors colors;
    if (!ab_theme_colors(theme, &colors, error)) {
        return false;
    }
    const AbDataTable *background = layer_list(theme, "bgLayers");
    const AbDataTable *foreground = layer_list(theme, "fgLayers");
    if (!background || !foreground) {
        return ab_fail(error,
                       "Theme requires static background/foreground layer tables");
    }
    size_t maximum = (allocation_limit - sizeof(AbThemeLayers)) /
                     (sizeof(AbThemeLayerBinding) + sizeof(uintptr_t));
    size_t background_count, foreground_count;
    if (!dense_count(background, maximum, &background_count, error) ||
        !dense_count(foreground, maximum - background_count, &foreground_count,
                     error)) {
        return false;
    }
    AbThemeLayers *layers = calloc(1, sizeof(*layers));
    if (!layers) {
        return ab_fail(error, "Cannot allocate theme layer owner");
    }
    layers->count = background_count + foreground_count;
    layers->background_count = background_count;
    layers->foreground_count = foreground_count;
    layers->colors = colors;
    layers->bytes = sizeof(*layers) + layers->count * (sizeof(*layers->bindings) +
                                                       sizeof(*layers->handles));
    if (layers->count) {
        layers->bindings = calloc(layers->count, sizeof(*layers->bindings));
        layers->handles = calloc(layers->count, sizeof(*layers->handles));
        if (!layers->bindings || !layers->handles) {
            ab_fail(error, "Cannot allocate theme layer bindings/handles");
            goto release_layers;
        }
    }
    for (size_t index = 0; index < layers->count; ++index) {
        const AbDataTable *list = index < background_count ? background : foreground;
        size_t slot =
            index < background_count ? index + 1 : index - background_count + 1;
        const AbDataValue *value = tuple(list, slot);
        if (!value || value->kind != AB_DATA_TABLE ||
            !bind_layer(value->table, &sources, layers->bindings + index, error)) {
            goto release_layers;
        }
        layers->handles[index] = index + 1;
    }
    *output = layers;
    return true;

release_layers:
    ab_theme_layers_close(layers);
    return false;
}

bool ab_theme_layers_open(const AbDataTable *theme, const AbSpriteSet *sprites,
                          const AbCompositeResources *composites,
                          size_t allocation_limit, AbThemeLayers **output,
                          AbError *error) {
    return open_layers(theme, (LayerSources){sprites, composites, NULL},
                       allocation_limit, output, error);
}

bool ab_theme_layers_open_provider(const AbDataTable *theme,
                                   const AbSpriteProvider *provider,
                                   size_t allocation_limit, AbThemeLayers **output,
                                   AbError *error) {
    return open_layers(theme, (LayerSources){.provider = provider}, allocation_limit,
                       output, error);
}

void ab_theme_layers_close(AbThemeLayers *layers) {
    if (layers) {
        free(layers->handles);
        free(layers->bindings);
        free(layers);
    }
}

size_t ab_theme_layers_count(const AbThemeLayers *layers) {
    return layers ? layers->count : 0;
}

size_t ab_theme_layers_bytes(const AbThemeLayers *layers) {
    return layers ? layers->bytes : 0;
}

const AbThemeColors *ab_theme_layers_colors(const AbThemeLayers *layers) {
    return layers ? &layers->colors : NULL;
}

bool ab_theme_layers_binding(const AbThemeLayers *layers, uintptr_t handle,
                             const AbThemeLayerBinding **output) {
    if (!layers || !output || !handle || handle > layers->count) {
        return false;
    }
    *output = layers->bindings + handle - 1;
    return true;
}

bool ab_theme_layers_background(const AbThemeLayers *layers,
                                AbThemeBackground *output) {
    if (!layers || !output) {
        return false;
    }
    AbThemeBackground view = {.layers = layers->handles,
                              .count = layers->background_count};
    memcpy(view.color, layers->colors.background, sizeof(view.color));
    *output = view;
    return true;
}

bool ab_theme_layers_foreground(const AbThemeLayers *layers,
                                AbThemeForeground *output) {
    if (!layers || !output) {
        return false;
    }
    AbThemeForeground view = {.layers = layers->foreground_count
                                            ? layers->handles + layers->background_count
                                            : NULL,
                              .count = layers->foreground_count};
    memcpy(view.ground_color, layers->colors.ground, sizeof(view.ground_color));
    if (view.count) {
        const AbThemeLayerDefinition *first =
            &layers->bindings[layers->background_count].definition;
        view.first = (AbThemeForegroundFirstLayer){true, first->height, first->pivot_y,
                                                   first->base.scale};
    }
    *output = view;
    return true;
}

typedef struct {
    const AbThemeLayerBinding *layer;
    AbThemeLayerImage image;
    void *context;
} BaseDraw;

static bool draw_sprite(BaseDraw *draw, const AbSpriteBinding *sprite, AbVec2 position,
                        AbVec2 scale, const float tint[4], AbError *error) {
    AbVec2 vertices[4];
    return ab_theme_sprite_vertices(sprite->region, position, scale, vertices, error) &&
           draw->image(draw->context, sprite, vertices, tint, error);
}

static bool draw_placement(void *context, AbVec2 position, AbVec2 scale,
                           const float tint[4], AbError *error) {
    BaseDraw *draw = context;
    if (draw->layer->kind == AB_THEME_SPRITE_ORDINARY) {
        return draw_sprite(draw, &draw->layer->ordinary, position, scale, tint, error);
    }
    const AbResolvedComposite *composite = &draw->layer->composite;
    for (size_t index = 0; index < composite->source->part_count; ++index) {
        const AbCompositePart *part = composite->source->parts + index;
        AbVec2 point = {position.x + (float)part->x, position.y + (float)part->y};
        if (!draw_sprite(draw, composite->parts + index, point, scale, tint, error)) {
            return false;
        }
    }
    return true;
}

bool ab_theme_layers_draw_base(const AbThemeLayers *layers, uintptr_t handle,
                               const AbThemeLayerDrawInput *input,
                               AbThemeLayerRenderState *state, AbThemeLayerImage image,
                               void *context, AbError *error) {
    const AbThemeLayerBinding *layer;
    if (!input || !state || !image ||
        !ab_theme_layers_binding(layers, handle, &layer)) {
        return ab_fail(error, "Invalid owned theme base draw arguments");
    }
    if (layer->kind == AB_THEME_SPRITE_BORDER)
        return ab_fail(error,
                       "Screen-border layers require their explicit draw method");
    if (!layer->definition.has_base_sprite) {
        return true;
    }
    AbThemeLayerTransform transform = {input->camera, input->viewport_width,
                                       layer->definition.parallax};
    BaseDraw draw = {layer, image, context};
    return ab_theme_layer_draw_base(&layer->definition.base, &transform, input->time,
                                    state, input->placement_limit, draw_placement,
                                    &draw, error);
}

bool ab_theme_layers_draw_border(const AbThemeLayers *layers, uintptr_t handle,
                                 uint32_t width, uint32_t height, uint32_t filter,
                                 AbThemeLayerRenderState *state,
                                 AbThemeLayerImage image, void *context,
                                 AbError *error) {
    const AbThemeLayerBinding *layer;
    if (!state || !image || !ab_theme_layers_binding(layers, handle, &layer) ||
        layer->kind != AB_THEME_SPRITE_BORDER || !width || !height ||
        width > INT32_MAX || height > INT32_MAX)
        return ab_fail(error, "Invalid screen-border draw arguments");
    if (filter)
        return true;
    *state = (AbThemeLayerRenderState){.geometry = {.scale_x = 1, .scale_y = 1},
                                       .alpha = layer->border_alpha};
    if (!layer->ordinary.region)
        return true;
    float w = (float)width, h = (float)height;
    float border = fminf(w, h) * layer->border_size;
    if (!isfinite(border) || !isfinite(w - border) || !isfinite(h - border))
        return ab_fail(error, "Screen-border geometry overflow");
    const AbVec2 points[4][4] = {{{w - border, 0}, {w, 0}, {w - border, h}, {w, h}},
                                 {{border, h}, {0, h}, {border, 0}, {0, 0}},
                                 {{w, h - border}, {w, h}, {0, h - border}, {0, h}},
                                 {{0, border}, {0, 0}, {w, border}, {w, 0}}};
    const float color[] = {255, 255, 255, layer->border_alpha * 255};
    for (size_t index = 0; index < 4; ++index)
        if (!image(context, &layer->ordinary, points[index], color, error))
            return false;
    return true;
}
