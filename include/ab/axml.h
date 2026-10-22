#ifndef AB_AXML_H
#define AB_AXML_H

#include "ab/base.h"

typedef struct {
    char package[128];
    char version_name[64];
    char launcher[192];
    uint32_t version_code;
    uint32_t min_sdk;
    uint32_t target_sdk;
    uint32_t gles_version;
    size_t activity_count;
    size_t service_count;
    size_t receiver_count;
} AbManifest;

/* Bounded Android binary XML reader; no resource-table resolution is implied. */
bool ab_manifest_read(AbSpan input, AbManifest *output, AbError *error);

#endif
