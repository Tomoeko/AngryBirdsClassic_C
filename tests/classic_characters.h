#ifndef AB_TEST_CLASSIC_CHARACTERS_H
#define AB_TEST_CLASSIC_CHARACTERS_H

#include "ab/classic_characters.h"

typedef struct {
    AbClassicLevelView level;
    size_t losses, distances, sounds, hats;
    bool lost;
} CharacterHostTest;

static bool character_lose(void *context, bool *output, AbError *error) {
    (void)error;
    CharacterHostTest *test = context;
    ++test->losses;
    *output = test->lost;
    return true;
}

static bool character_distance(void *context, size_t object,
                               AbClassicCharacterKind kind, float *output,
                               AbError *error) {
    (void)error;
    CharacterHostTest *test = context;
    CHECK(object < test->level.bodies->count);
    const AbLevelObject *source = test->level.bodies->objects + object;
    CHECK(kind == AB_CLASSIC_CHARACTER_GOAL ? source->level_goal
                                            : source->physics.controllable);
    ++test->distances;
    *output = kind == AB_CLASSIC_CHARACTER_GOAL ? 0 : 2000;
    return true;
}

static bool character_sound(void *context, size_t object, AbClassicCharacterKind kind,
                            float volume, AbError *error) {
    (void)error;
    CharacterHostTest *test = context;
    CHECK(object < test->level.bodies->count && kind == AB_CLASSIC_CHARACTER_GOAL &&
          volume == 1);
    ++test->sounds;
    return true;
}

static bool character_hat(void *context, size_t object, AbError *error) {
    (void)error;
    CharacterHostTest *test = context;
    CHECK(object < test->level.bodies->count);
    ++test->hats;
    return true;
}

static bool character_name(const AbClassicVisuals *visuals, size_t object,
                           AbSpan name) {
    const AbObjectVisual *view = ab_classic_visuals_object(visuals, object);
    return view && view->sprite_present && view->sprite.size == name.size &&
           (!name.size || memcmp(view->sprite.data, name.data, name.size) == 0);
}

static bool character_random_advance(AbRandom48 *random, size_t count) {
    uint32_t unused;
    for (size_t index = 0; index < count; ++index) {
        CHECK(ab_random48_next(random, &unused));
    }
    return true;
}

static bool character_damage_faces(AbClassicLevelView level, AbClassicVisuals *visuals,
                                   AbClassicCharacters *characters,
                                   CharacterHostTest *host) {
    size_t pig = level.goal_order[0];
    const char *const keys[] = {"strength", "damageSprite", "blinkSprite",
                                "smileSprite", "sprite"};
    AbDataValue saved[5];
    for (size_t index = 0; index < 5; ++index)
        CHECK(ab_level_property_get(level.properties, pig, string(keys[index]),
                                    saved + index, NULL));
    for (size_t index = 0; index < level.bird_count; ++index)
        level.character_objects[level.bird_order[index]].shot = true;
    AbClassicCharacterView view = ab_classic_characters_view(characters);
    AbCharacterAnimationObject *object = level.character_objects + pig;
    object->override_drawing = false;
    object->goal.special_expression = false;
    object->goal.oink_timer = 1000;
    view.bindings[pig].goal.disable_smiling = false;
    size_t indices[16];
    AbStringOrderNode nodes[16];
    AbStringOrderWorkspace workspace = {indices, 16, nodes, 16};
    const float health[] = {2, 1, 0.25f};
    for (size_t stage = 0; stage < 3; ++stage) {
        AbDamageSpriteNames names;
        CHECK(ab_level_property_set(
                  level.properties, pig, string("strength"),
                  (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(health[stage])},
                  NULL) &&
              ab_classic_visuals_update_damage(visuals, pig, &workspace, &names, NULL));
        AbDataValue faces[] = {names.sprite, names.blink, names.smile};
        for (size_t index = 0; index < 3; ++index)
            CHECK(ab_level_property_set(level.properties, pig, string(keys[index + 1]),
                                        faces[index], NULL));
        CHECK(names.sprite.kind == AB_DATA_STRING &&
              !character_name(visuals, pig, level.initial_sprites[pig].sprite.string));
        host->lost = false;
        for (size_t frame = 0; frame < 12; ++frame) {
            object->goal.blink_timer = -1;
            CHECK(ab_classic_characters_update(characters, 1.0f / 60, NULL) &&
                  character_name(visuals, pig,
                                 object->goal.blink_on ? names.blink.string
                                                       : names.sprite.string));
        }
        host->lost = true;
        CHECK(ab_classic_characters_update(characters, 1.0f / 60, NULL) &&
              character_name(visuals, pig, names.smile.string));
    }
    for (size_t index = 0; index < 5; ++index)
        CHECK(ab_level_property_set(level.properties, pig, string(keys[index]),
                                    saved[index], NULL));
    CHECK(ab_classic_visuals_set_sprite(visuals, pig, saved[4].string, NULL));
    puts("Pig damage faces retain each health range through blinking and smiling, "
         "including the final range before death.");
    return true;
}

static bool character_owner_checks(AbClassicLevel *level, AbClassicVisuals *visuals,
                                   AbClassicScene *scene,
                                   AbClassicCharacters **output) {
    AbClassicLevelView loaded = ab_classic_level_view(level);
    CharacterHostTest test = {.level = loaded};
    AbClassicCharacterHost host = {&test, character_lose, character_distance,
                                   character_sound, character_hat};
    AbClassicCharacters *characters = NULL;
    AbRandom48 random;
    CHECK(ab_random48_seed(&random, 17) && !loaded.timers_initialized &&
          !ab_classic_characters_open(level, visuals, &random, host, 65536, &characters,
                                      NULL) &&
          !characters &&
          ab_classic_level_initialize_objects(level, &random, false, NULL));
    AbRandom48 after_startup = random;
    CHECK(
        !ab_classic_characters_open(NULL, visuals, &random, host, 65536, &characters,
                                    NULL) &&
        !characters &&
        !ab_classic_characters_open(level, NULL, &random, host, 65536, &characters,
                                    NULL) &&
        !ab_classic_characters_open(level, visuals, NULL, host, 65536, &characters,
                                    NULL) &&
        !ab_classic_characters_open(level, visuals, &random, host, 65536, NULL, NULL) &&
        !ab_classic_characters_open(level, visuals, &random, host, 1, &characters,
                                    NULL));
    CHECK(ab_classic_characters_open(level, visuals, &random, host, 65536, &characters,
                                     NULL));
    size_t bytes = ab_classic_characters_bytes(characters);
    ab_classic_characters_close(characters);
    characters = NULL;
    CHECK(ab_classic_characters_open(level, visuals, &random,
                                     (AbClassicCharacterHost){0}, bytes, &characters,
                                     NULL));
    AbError missing_host = {0};
    CHECK(!ab_classic_characters_update(characters, 0, &missing_host) &&
          strstr(missing_host.message, "lose-condition") &&
          random.value == after_startup.value);
    ab_classic_characters_close(characters);
    characters = NULL;
    CHECK(!ab_classic_characters_open(level, visuals, &random, host, bytes - 1,
                                      &characters, NULL) &&
          !characters &&
          ab_classic_characters_open(level, visuals, &random, host, bytes, &characters,
                                     NULL));
    *output = characters;
    AbClassicCharacterView view = ab_classic_characters_view(characters);
    CHECK(view.count == 23 && view.animations->objects == loaded.character_objects &&
          view.animations->goal_order == loaded.goal_order &&
          view.animations->bird_order == loaded.bird_order &&
          random.value == after_startup.value && !test.losses && !test.distances &&
          !test.sounds && ab_classic_characters_bytes(characters) == bytes &&
          !ab_classic_characters_bytes(NULL) &&
          !ab_classic_characters_view(NULL).animations &&
          !ab_classic_characters_update(NULL, 0, NULL));
    CHECK(loaded.goal_count == 1 && loaded.bird_count == 3);
    size_t goal = loaded.goal_order[0];
    CHECK(view.bindings[goal].goal.base_strength == 4 &&
          view.bindings[goal].goal.damage1_min == 90);
    loaded.character_objects[goal].goal.blink_timer = -1;
    loaded.character_objects[goal].goal.oink_timer = 1000;
    view.bindings[goal].goal.disable_smiling = true;
    for (size_t cursor = 0; cursor < loaded.bird_count; ++cursor) {
        size_t index = loaded.bird_order[cursor];
        AbCharacterAnimationObject *object = loaded.character_objects + index;
        object->bird.timer = -1;
        object->jump.timer = 1000;
        CHECK(view.bindings[index].bird.blink_sprite_present &&
              view.bindings[index].bird.yell_sprite_present);
    }
    AbRandom48 expected = random;
    CHECK(character_random_advance(&expected, 1 + 2 * loaded.bird_count));
    CHECK(ab_classic_characters_update(characters, 0, NULL) &&
          random.value == expected.value &&
          loaded.character_objects[goal].goal.blink_on &&
          character_name(visuals, goal, view.sprites[goal].damage.blink.string));
    for (size_t cursor = 0; cursor < loaded.bird_count; ++cursor) {
        size_t index = loaded.bird_order[cursor];
        CHECK(loaded.character_objects[index].bird.active &&
              (character_name(visuals, index, string("BIRD_RED_BLINK")) ||
               character_name(visuals, index, string("BIRD_RED_YELL"))));
        loaded.character_objects[index].bird.timer = -1;
    }
    loaded.character_objects[goal].goal.blink_timer = -1;
    CHECK(character_random_advance(&expected, 1 + loaded.bird_count));
    CHECK(ab_classic_characters_update(characters, 0, NULL) &&
          random.value == expected.value &&
          !loaded.character_objects[goal].goal.blink_on &&
          character_name(visuals, goal, view.sprites[goal].damage.sprite.string));
    for (size_t cursor = 0; cursor < loaded.bird_count; ++cursor) {
        size_t index = loaded.bird_order[cursor];
        CHECK(!loaded.character_objects[index].bird.active &&
              character_name(visuals, index, view.sprites[index].damage.sprite.string));
        loaded.character_objects[index].shot = true;
    }
    test.lost = true;
    view.bindings[goal].goal.disable_smiling = false;
    loaded.character_objects[goal].goal.oink_timer = -1;
    CHECK(character_random_advance(&expected, 1));
    CHECK(ab_classic_characters_update(characters, 0, NULL) && test.losses == 1 &&
          test.sounds == 1 && random.value == expected.value &&
          character_name(visuals, goal, view.sprites[goal].damage.smile.string));
    loaded.character_objects[goal].goal.special_expression = true;
    loaded.character_objects[goal].goal.special_timer = 1;
    view.sprites[goal].special_expression = view.sprites[goal].damage.blink;
    CHECK(ab_classic_characters_update(characters, 0, NULL) && test.losses == 1 &&
          character_name(visuals, goal, view.sprites[goal].damage.blink.string) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL));
    loaded.character_objects[goal].override_drawing = true;
    size_t bird = loaded.bird_order[0];
    AbCharacterAnimationObject *object = loaded.character_objects + bird;
    object->shot = false;
    object->bird.timer = 1000;
    object->jump.timer = -1;
    object->has_hat = true;
    CHECK(ab_classic_characters_update(characters, 0.1f, NULL) &&
          object->jump.jumping && test.hats == 1 &&
          object->jump.position.y == loaded.frame->objects[bird].rendered.position.y &&
          object->jump.position.y ==
              loaded.world->storage.bodies[loaded.frame->objects[bird].body]
                  .motion.mass.transform.position.y);
    CHECK(character_damage_faces(loaded, visuals, characters, &test));
    /* The host is test-local; release before it or the borrowed RNG expires. */
    ab_classic_characters_close(characters);
    *output = NULL;
    ab_classic_characters_close(NULL);
    puts("Owned APK character bindings update real face sprites, shared random state, "
         "bodies/history and ordered loss/audio/hat boundaries.");
    return true;
}

#endif
