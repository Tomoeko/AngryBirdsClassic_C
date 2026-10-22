#ifndef AB_TEST_CLASSIC_DAMAGE_VISUALS_H
#define AB_TEST_CLASSIC_DAMAGE_VISUALS_H

static bool damage_name_equal(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    if (first.kind == AB_DATA_NIL)
        return true;
    return first.kind == AB_DATA_STRING && first.string.size == second.string.size &&
           (!first.string.size ||
            !memcmp(first.string.data, second.string.data, first.string.size));
}

static bool shot_damage_visuals(ShotTest *test, AbClassicVisuals *visuals,
                                AbError *error) {
    size_t indices[16];
    AbStringOrderNode nodes[16];
    AbStringOrderWorkspace workspace = {indices, 16, nodes, 16};
    for (size_t index = 0; index < test->level.bodies->count; ++index) {
        AbDamageSpriteNames names;
        CHECK(
            ab_classic_visuals_damage_names(visuals, index, &workspace, &names, error));
        const AbDataTable *physical = &test->level.properties->objects[index].physical;
        CHECK(damage_name_equal(names.sprite, field(physical, "damageSprite")) &&
              damage_name_equal(names.blink, field(physical, "blinkSprite")) &&
              damage_name_equal(names.smile, field(physical, "smileSprite")));
    }
    CHECK(test->level.goal_count == 1);
    size_t pig = test->level.goal_order[0];
    const AbDataTable *physical = &test->level.properties->objects[pig].physical;
    AbDataValue strength = field(physical, "strength");
    AbDataValue initial = field(physical, "initialStrength");
    AbDataValue original = field(physical, "sprite");
    AbDataValue frozen = field(physical, "frozenSolid");
    AbDataValue skip = field(physical, "dontUpdateDamageSprite");
    AbDamageSpriteNames names;
    AbLevelPropertyObject *properties = test->level.properties->objects + pig;
    AbDataValue script_physical = properties->script_entries[1].value;
    properties->script_entries[1].value = (AbDataValue){0};
    bool canonical =
        ab_classic_visuals_damage_names(visuals, pig, &workspace, &names, error);
    properties->script_entries[1].value = script_physical;
    CHECK(canonical && damage_name_equal(names.sprite, original));
    AbDataValue definition = field(physical, "definition");
    CHECK(ab_level_property_set(test->level.properties, pig, span("definition"),
                                (AbDataValue){0}, error) &&
          ab_classic_visuals_damage_names(visuals, pig, NULL, &names, error) &&
          damage_name_equal(names.sprite, original) &&
          ab_level_property_set(test->level.properties, pig, span("definition"),
                                definition, error));
    CHECK(strength.kind == AB_DATA_NUMBER && strength.bits == bits(4) &&
          ab_level_property_set(test->level.properties, pig, span("strength"),
                                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(2)},
                                error) &&
          ab_classic_visuals_damage_names(visuals, pig, &workspace, &names, error) &&
          !damage_name_equal(names.sprite, original));
    AbDamageSpriteNames saved = names;
    CHECK(!ab_classic_visuals_damage_names(visuals, pig, NULL, &names, NULL) &&
          !memcmp(&saved, &names, sizeof(names)));
    CHECK(ab_level_property_set(test->level.properties, pig, span("frozenSolid"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                error) &&
          ab_classic_visuals_update_damage(visuals, pig, &workspace, &names, error) &&
          damage_name_equal(field(physical, "sprite"), original) &&
          ab_level_property_set(test->level.properties, pig, span("frozenSolid"),
                                frozen, error) &&
          ab_level_property_set(
              test->level.properties, pig, span("dontUpdateDamageSprite"),
              (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) &&
          ab_classic_visuals_update_damage(visuals, pig, &workspace, &names, error) &&
          damage_name_equal(field(physical, "sprite"), original) &&
          ab_level_property_set(test->level.properties, pig,
                                span("dontUpdateDamageSprite"), skip, error) &&
          ab_classic_visuals_update_damage(visuals, pig, &workspace, &names, error) &&
          damage_name_equal(field(physical, "sprite"), names.sprite) &&
          damage_name_equal(field(physical, "damageSprite"), original));
    /* The collision callback publishes its damage-name cache after this real
     * visual boundary; getDamageSprite itself must leave that cache untouched. */
    CHECK(ab_level_property_set(test->level.properties, pig, span("initialStrength"),
                                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(2)},
                                error) &&
          ab_classic_visuals_update_damage(visuals, pig, &workspace, &names, error) &&
          damage_name_equal(names.sprite, original) &&
          ab_level_property_set(test->level.properties, pig, span("initialStrength"),
                                initial, error) &&
          ab_level_property_set(test->level.properties, pig, span("strength"), strength,
                                error));
    puts("Canonical damage names, health/initial-strength changes and frozen/skip "
         "visual gates execute through the real visual owner");
    return true;
}

#endif
