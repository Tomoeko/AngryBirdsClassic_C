#ifndef AB_DAMAGE_SPRITE_H
#define AB_DAMAGE_SPRITE_H

#include "ab/lua_data.h"
#include "ab/object_definition.h"
#include "ab/data_order.h"

typedef struct {
    AbDataValue sprite;
    AbDataValue blink;
    AbDataValue smile;
    AbDataValue frozen;
} AbDamageSpriteNames;

/* Numeric/range portion of getDamageSpriteNames. The caller resolves definition
 * and catalog availability, then selects truthy object.damageSprites or the
 * definition's ranges; NULL means no range table. range_order is its complete
 * native pairs permutation, with table.count entries. It is borrowed/disjoint
 * from output. Select the first range satisfying min < strength/base*100 <= max.
 * A truthy initial_strength replaces definition_strength, including numeric zero.
 * Nil strength, absent ranges or no matching interval retain base_sprite for
 * sprite/blink/smile and nil for frozen. Non-nil optional blinkSprite,
 * smileSprite and frozenSprite values replace defaults, including false.
 * Output values borrow all source data. Used numeric inputs must be finite;
 * ratio division by zero and overflow preserve IEEE comparison results. No
 * allocation, table construction, rendering or original code execution occurs.
 * Invalid used data/order preserves output; unvisited range contents are unused. */
bool ab_damage_sprite_names(AbDataValue base_sprite, AbDataValue strength,
                            AbDataValue initial_strength,
                            AbDataValue definition_strength, const AbDataTable *ranges,
                            const size_t *range_order, AbDamageSpriteNames *output,
                            AbError *error);

/* Prepare names for an ordinary newly created definition with no instance
 * range/initialStrength override. Both strength operands use the constructor's
 * normalized material fallback while the input catalog stays immutable. Range order
 * comes from that catalog data's static constructor layout. Caller supplies disjoint
 * scratch for its complete hash capacity and entry count. Returned values
 * borrow the catalog. No sprite action is emitted or object field assigned. */
bool ab_object_initial_sprites(const AbObjectDefinition *definition,
                               const AbLuaData *catalog,
                               AbStringOrderWorkspace *workspace,
                               AbDamageSpriteNames *output, AbError *error);

#endif
