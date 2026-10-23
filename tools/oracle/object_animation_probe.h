#ifndef AB_ORACLE_OBJECT_ANIMATION_H
#define AB_ORACLE_OBJECT_ANIMATION_H

#include "selected_function.h"
#include "../../tests/object_animation_cases.h"

static void object_animation_snapshot(void *lua) {
    const char *const fields[] = {"animationTimer", "currentAnimationFrame",
                                  "animationFinished", "damageSprites", "damageSprite"};
    uint32_t words[10];
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, 1, fields[field]);
        int type = damage_lua.type(lua, -1);
        words[2 * field + 1] = 0;
        if (type == 0) {
            words[2 * field] = 0;
        } else if (type == 1) {
            words[2 * field] = 1;
            words[2 * field + 1] = (uint32_t)damage_lua.to_boolean(lua, -1);
        } else if (type == 3) {
            words[2 * field] = 2;
            FloatBits number = {.number = damage_lua.to_number(lua, -1)};
            words[2 * field + 1] = number.bits;
        } else if (type == 4 && damage_named(lua, -1, "base")) {
            words[2 * field] = 3;
            words[2 * field + 1] = 1;
        } else {
            fail("Unexpected original object animation field\n");
        }
        damage_lua.set_top(lua, 1);
    }
    print_words(words, 10);
}

static void probe_object_animation(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original object animation oracle state\n");
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *const names[] = {"_ab_object_animation", "getDamageSpriteNames",
                                 "getDamageSprite"};
    const char *const paths[] = {"/data/local/tmp/ab520/object-animation-update.lua",
                                 "/data/local/tmp/ab520/object-animation-names.lua",
                                 "/data/local/tmp/ab520/object-animation-damage.lua"};
    for (size_t index = 0; index < 3; ++index)
        load_selected_function(lua, paths[index], names[index]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    semihost(4, "AB_ANDROID_OBJECT_ANIMATION_V1 256 8\n");
    for (size_t item = 0; item < AB_OBJECT_ANIMATION_CASES; ++item) {
        AbObjectAnimationCase input = ab_object_animation_case(item);
        damage_lua.create_table(lua, 0, 12);
        damage_lua.push_string(lua, "base");
        damage_lua.set_field(lua, 1, "sprite");
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "dontUpdateDamageSprite");
        damage_lua.push_boolean(lua, (int)input.finished);
        damage_lua.set_field(lua, 1, "animationFinished");
        damage_lua.push_boolean(lua, (int)input.disabled);
        damage_lua.set_field(lua, 1, "animationDisabled");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "damageSprite");
        damage_lua.push_number(lua, 13);
        damage_lua.set_field(lua, 1, "damageSprites");
        damage_lua.push_number(lua, input.frame);
        damage_lua.set_field(lua, 1, "currentAnimationFrame");
        if (input.timer_present) {
            damage_lua.push_number(lua, input.timer);
            damage_lua.set_field(lua, 1, "animationTimer");
        }
        damage_lua.create_table(lua, 0, 10);
        if (input.duration_kind == 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 2, "timePerFrame");
        } else if (input.duration_kind == 2) {
            damage_lua.push_number(lua, input.duration);
            damage_lua.set_field(lua, 2, "timePerFrame");
        }
        damage_lua.push_boolean(lua, (int)input.looping);
        damage_lua.set_field(lua, 2, "looping");
        for (unsigned frame = 1; frame <= input.frames; ++frame) {
            char key[] = "damageSprites0";
            key[13] = (char)('0' + frame);
            if (frame == input.false_frame)
                damage_lua.push_boolean(lua, 0);
            else
                damage_lua.push_number(lua, (float)frame);
            damage_lua.set_field(lua, 2, key);
        }
        if (!input.active) {
            damage_lua.set_top(lua, 1);
            damage_lua.push_boolean(lua, 0);
        }
        damage_lua.set_field(lua, 1, "animation");
        for (size_t step = 0; step < AB_OBJECT_ANIMATION_STEPS; ++step) {
            damage_lua.get_field(lua, -10002, names[0]);
            damage_lua.push_nil(lua);
            push_value(lua, 1);
            damage_lua.push_number(lua, input.seconds[step]);
            if (damage_lua.call(lua, 3, 0, 0)) {
                semihost(4, damage_lua.to_string(lua, -1, NULL));
                fail("Original object animation call failed\n");
            }
            object_animation_snapshot(lua);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB object animation reference complete\n");
}

#endif
