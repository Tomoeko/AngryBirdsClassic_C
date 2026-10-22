#ifndef AB_REFERENCE_GUI_QUERIES_H
#define AB_REFERENCE_GUI_QUERIES_H

typedef struct {
    unsigned root, pause, notification_visible, notification_pause;
} ReferenceGuiQuery;

static ReferenceGuiQuery reference_gui_query(unsigned profile) {
    return (ReferenceGuiQuery){profile & 3, (profile >> 2) & 3, (profile >> 4) & 3,
                               (profile >> 6) & 3};
}

#endif
