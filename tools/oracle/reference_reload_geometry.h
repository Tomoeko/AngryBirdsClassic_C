#ifndef AB_REFERENCE_RELOAD_GEOMETRY_H
#define AB_REFERENCE_RELOAD_GEOMETRY_H

/* Bird/castle snap coordinates; published screen center is zero. Inputs only. */
static void reference_reload_geometry(unsigned profile, float coordinates[4]) {
    for (unsigned index = 0; index < 4; ++index) {
        unsigned mixed = (profile + 1) * (index * 317 + 977);
        int value = (int)(mixed % 131071) - 65535;
        coordinates[index] = (float)value / (profile & 1 ? 8.0f : 128.0f);
    }
    if (!profile)
        for (unsigned index = 0; index < 4; ++index)
            coordinates[index] = 0;
    if (profile == 1) {
        coordinates[2] = coordinates[0];
        coordinates[3] = coordinates[1];
    }
}

#endif
