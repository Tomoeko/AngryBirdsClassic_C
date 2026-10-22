#include "ab/ui_widget_resources.h"

#include <string.h>

static bool valid(const AbUiWidgetResources *resources, AbSpan image, AbError *error) {
    return resources && resources->initialized && resources->sprites &&
                   resources->strings && (!image.size || image.data)
               ? true
               : ab_fail(error, "Invalid concrete UI resource request");
}

static bool localize(void *context, AbSpan requested, AbSpan *output, AbError *error) {
    AbUiWidgetResources *resources = context;
    return valid(resources, requested, error) &&
           ab_ui_localization_string(resources->strings, requested, NULL, output,
                                     error);
}

static bool bounds(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    AbUiWidgetResources *resources = context;
    if (!output || !valid(resources, image, error))
        return false;
    AbSpriteAtlasBinding sprite;
    AbResolvedComposite composite;
    const AbSpriteProvider *provider = resources->sprites;
    if (provider->sprite(provider->context, image, &sprite)) {
        if (!sprite.sprite.region)
            return ab_fail(error, "Invalid concrete UI sprite bounds");
        *output = (AbVec2){sprite.sprite.region->width, sprite.sprite.region->height};
        return true;
    }
    if (provider->composite &&
        provider->composite(provider->context, image, &composite)) {
        *output =
            (AbVec2){(float)composite.bounds.width, (float)composite.bounds.height};
        return true;
    }
    /* Target Resources::getSpriteWidth/Height, 0x59bc98/0x59bce0, return
     * zero for an absent or unsupported sprite. Legacy episode icon queries
     * intentionally reach this path with the modern resource set. */
    *output = (AbVec2){0};
    return true;
}

static bool pivot(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    AbUiWidgetResources *resources = context;
    if (!output || !valid(resources, image, error))
        return false;
    AbSpriteAtlasBinding sprite;
    AbResolvedComposite composite;
    const AbSpriteProvider *provider = resources->sprites;
    if (provider->sprite(provider->context, image, &sprite)) {
        if (!sprite.sprite.region)
            return ab_fail(error, "Invalid concrete UI sprite pivot");
        *output =
            (AbVec2){sprite.sprite.region->pivot_x, sprite.sprite.region->pivot_y};
        return true;
    }
    if (provider->composite &&
        provider->composite(provider->context, image, &composite)) {
        *output =
            (AbVec2){(float)composite.bounds.pivot_x, (float)composite.bounds.pivot_y};
        return true;
    }
    /* Resources::getSpritePivotX/Y, 0x59bd28/0x59bd70. */
    *output = (AbVec2){0};
    return true;
}

static bool render_binding(AbUiWidgetResources *resources, AbSpriteAtlasBinding sprite,
                           AbUiImageRenderBinding *output, AbError *error) {
    const AbSpriteProvider *provider = resources->sprites;
    if (!sprite.sprite.region || !sprite.image)
        return ab_fail(error, "Missing concrete UI image render binding");
    uint32_t texture;
    if (!provider->texture(provider->context, sprite.image, &texture) || !texture)
        return ab_fail(error, "Missing synchronized concrete UI image texture");
    *output = (AbUiImageRenderBinding){.region = sprite.sprite.region,
                                       .texture = texture,
                                       .alpha = sprite.alpha,
                                       .texture_width = sprite.image->width,
                                       .texture_height = sprite.image->height};
    return true;
}

static bool lookup(void *context, AbSpan image, AbUiImageRenderBinding *output,
                   AbError *error) {
    AbUiWidgetResources *resources = context;
    if (!output || !valid(resources, image, error))
        return false;
    const AbSpriteProvider *provider = resources->sprites;
    AbSpriteAtlasBinding sprite;
    AbResolvedComposite composite;
    if (provider->sprite(provider->context, image, &sprite))
        return render_binding(resources, sprite, output, error);
    if (provider->composite &&
        provider->composite(provider->context, image, &composite)) {
        *output = (AbUiImageRenderBinding){.composite = composite};
        return true;
    }
    /* Resources::drawSprite (BN ELF VA0x59bbd8) checks findSprite before
     * dispatch. Legacy UI names may be absent from the selected APK profile. */
    *output = (AbUiImageRenderBinding){0};
    return true;
}

static bool part(void *context, const AbSpriteBinding *binding,
                 AbUiImageRenderBinding *output, AbError *error) {
    AbUiWidgetResources *resources = context;
    if (!resources || !resources->initialized || !binding || !output)
        return ab_fail(error, "Invalid concrete UI composite part request");
    const AbSpriteProvider *provider = resources->sprites;
    AbSpriteAtlasBinding sprite;
    if (!provider->binding || !provider->binding(provider->context, *binding, &sprite))
        return ab_fail(error, "Missing cached concrete UI composite part pixels");
    return render_binding(resources, sprite, output, error);
}

static bool play(void *context, AbUiPointerValue sound, float volume, bool loop,
                 AbError *error) {
    AbUiWidgetResources *resources = context;
    if (!resources || !resources->initialized || !resources->audio ||
        !ab_ui_pointer_value_valid(&sound))
        return ab_fail(error, "Missing concrete UI audio bank");
    AbAudioPlay request = {.volume = {.kind = AB_DATA_NUMBER},
                           .loop = {.kind = AB_DATA_BOOLEAN, .bits = loop}};
    memcpy(&request.volume.bits, &volume, sizeof(volume));
    switch (sound.kind) {
        case AB_UI_POINTER_NONE:
            break;
        case AB_UI_POINTER_BOOLEAN:
            request.name =
                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = sound.boolean};
            break;
        case AB_UI_POINTER_NUMBER:
            request.name =
                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = sound.number_bits};
            break;
        case AB_UI_POINTER_TEXT:
            request.name = (AbDataValue){.kind = AB_DATA_STRING, .string = sound.text};
            break;
        default:
            return ab_fail(error, "Unsupported concrete UI audio identifier");
    }
    uint32_t handle;
    return ab_audio_bank_play(resources->audio, request, &handle, error);
}

bool ab_ui_widget_resources_initialize(const AbSpriteProvider *sprites,
                                       const AbUiLocalizationSource *strings,
                                       AbAudioBank *audio, AbUiWidgetResources *adapter,
                                       AbUiWidgetContext *context, AbError *error) {
    if (!sprites || !sprites->sprite || !sprites->texture || !strings ||
        !strings->get_string || !adapter || !context || context->failed)
        return ab_fail(error, "Invalid concrete UI resource providers");
    *adapter = (AbUiWidgetResources){sprites, strings, audio, true};
    context->images = (AbUiImageResourceBoundaries){adapter, localize, bounds, pivot};
    context->render.context = adapter;
    context->render.lookup = lookup;
    context->render.part = part;
    context->audio_context = adapter;
    context->play_audio = play;
    return true;
}
