#ifndef AB_TEST_POWERUP_USES_CASES_H
#define AB_TEST_POWERUP_USES_CASES_H

enum { POWERUP_USE_CASES = 8, POWERUP_USE_ACTIONS = 16 };
enum { POWERUP_USE_QUERY, POWERUP_USE_MARK, POWERUP_USE_CLEAR };

typedef struct {
    unsigned operation;
    const char *name;
} PowerupUseAction;

static const PowerupUseAction powerup_use_actions[POWERUP_USE_ACTIONS] = {
    {POWERUP_USE_QUERY, "KingSling"},
    {POWERUP_USE_MARK, "KingSling"},
    {POWERUP_USE_QUERY, "KINGSLING"},
    {POWERUP_USE_MARK, "KingSling"},
    {POWERUP_USE_MARK, "birdQuake"},
    {POWERUP_USE_QUERY, "powerPotion"},
    {POWERUP_USE_MARK, ""},
    {POWERUP_USE_QUERY, ""},
    {POWERUP_USE_QUERY, "unknown"},
    {POWERUP_USE_MARK, "PowerPotion"},
    {POWERUP_USE_QUERY, "powerpotion"},
    {POWERUP_USE_MARK, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_09-!?"},
    {POWERUP_USE_QUERY, "abcdefghijklmnopqrstuvwxyz_09-!?"},
    {POWERUP_USE_CLEAR, ""},
    {POWERUP_USE_QUERY, "birdquake"},
    {POWERUP_USE_MARK, "REDsMIGHTYFEATHERS"},
};

static const char *const powerup_use_initial_names[] = {"KINGSLING", "BirdQuake", ""};
static const char *const powerup_use_queries[] = {"powerpotion", "kingsling",
                                                  "birdquake"};

#endif
