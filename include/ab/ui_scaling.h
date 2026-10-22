#ifndef AB_UI_SCALING_H
#define AB_UI_SCALING_H

#include "ab/event_manager.h"
#include "ab/ui_image_resources.h"
#include "ab/ui_init.h"

typedef struct {
    AbVec2 screen, reference_size;
    bool use_scaling, template_layout;
} AbUiScalingEnvironment;

typedef struct {
    AbEventManager *events;
    AbDataValue resolution_event;
    void *context;
    bool (*environment)(void *context, AbUiScalingEnvironment *output, AbError *error);
} AbUiScalingHost;

typedef struct {
    AbUiScalingHost host;
    AbEventListener listener;
    AbVec2 default_profile;
    float default_aspect_ratio, default_scaling;
    bool initialized, registered, busy, failed;
} AbUiScaling;

/* Source Scaling owner. Register with the actual EventManager before selecting
 * its initial Android default profile (or the supplied ReferenceScaler size).
 * The profile stays fixed on resolution changes; useScaling gates event-driven
 * recalculation. environment reads live application flags/logical dimensions,
 * not framebuffer size. Finite positive used dimensions are required. No guessed
 * profile, frame allocation or VM. Keep this owner/host/event ID stable until
 * detach; reached failures retain registration and earlier source assignments.
 * detach is lifetime cleanup, not an emulated script callback. */
bool ab_ui_scaling_init(AbUiScaling *scaling, AbUiScalingHost host, AbError *error);
bool ab_ui_scaling_update(AbUiScaling *scaling, AbError *error);
bool ab_ui_scaling_detach(AbUiScaling *scaling, AbError *error);
bool ab_ui_scaling_get(const AbUiScaling *scaling, float *output, AbError *error);
bool ab_ui_scaling_set_to(const AbUiScaling *scaling, AbUiFrameFields *frame,
                          AbError *error);
bool ab_ui_scaling_sprite(const AbUiScaling *scaling,
                          const AbUiImageResourceBoundaries *resources, AbSpan image,
                          bool pivot, AbVec2 *output, AbError *error);

#endif
