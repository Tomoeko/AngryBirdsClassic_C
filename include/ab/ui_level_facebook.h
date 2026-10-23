#ifndef AB_UI_LEVEL_FACEBOOK_H
#define AB_UI_LEVEL_FACEBOOK_H

#include "ab/ui_level_button.h"

typedef struct {
    AbUiWidget like;
    AbUiScaling *scaling;
    const AbDataTable *page;
    char name[32];
    bool present;
} AbUiLevelFacebook;

/* LSFacebookLayout8492/8889/9396/10130. Its ordinary level buttons are always
 * open (isLevelOpen9344), independently of social/network services. Borrow the
 * actual catalog, scroll and scaling. The host supplies the live settings age
 * filter. Retain conditional Like-button creation, entry visibility, basic art
 * override and scaled row placement. The root owns GOTO_FACEBOOK_CONNECT; no
 * authentication, award or SDK callback is fabricated. No allocation or VM. */
bool ab_ui_level_facebook_init(AbUiLevelFacebook *owner, AbUiWidget *scroll,
                               const AbDataTable *page, AbUiScaling *scaling,
                               bool age_filter, AbError *error);
void ab_ui_level_facebook_entry(AbUiLevelFacebook *owner, bool age_filter);
bool ab_ui_level_facebook_layout(AbUiLevelFacebook *owner, AbUiLevelButton *buttons,
                                 size_t count, AbError *error);
bool ab_ui_level_facebook_open(AbUiLevelButton *button, bool age_filter,
                               AbError *error);

#endif
