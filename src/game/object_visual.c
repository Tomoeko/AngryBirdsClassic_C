#include "ab/object_visual.h"

static bool valid_span(AbSpan name) {
    return !name.size || name.data;
}

bool ab_sprite_asset_set(AbSpriteAssetState *state, AbSpan name, AbSpriteAssetHost host,
                         AbError *error) {
    if (!state || !valid_span(name)) {
        return ab_fail(error, "Invalid sprite asset assignment");
    }
    state->name = name;
    state->sprite = NULL;
    state->composite = NULL;
    if (!name.size) {
        return true;
    }
    if (!host.sprite) {
        return ab_fail(error, "Missing ordinary sprite lookup");
    }
    const AbSpriteRegion *sprite = NULL;
    if (!host.sprite(host.context, name, &sprite, error)) {
        return false;
    }
    state->sprite = sprite;
    if (sprite) {
        return true;
    }
    if (!host.composite) {
        return ab_fail(error, "Missing composite sprite lookup");
    }
    const AbComposite *composite = NULL;
    if (!host.composite(host.context, name, &composite, error)) {
        return false;
    }
    state->composite = composite;
    if (!composite && host.missing) {
        host.missing(host.context, name);
    }
    return true;
}

bool ab_object_visual_set_sprite(AbObjectVisual *object, AbSpan name,
                                 AbSpriteAssetHost host, AbError *error) {
    if (!object || !valid_span(name)) {
        return ab_fail(error, "Invalid object sprite assignment");
    }
    object->sprite = name;
    object->sprite_present = true;
    return !object->attached ||
           ab_sprite_asset_set(object->attached, name, host, error);
}
