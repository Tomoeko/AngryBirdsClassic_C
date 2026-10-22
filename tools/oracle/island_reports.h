#ifndef AB_ORACLE_ISLAND_REPORTS_H
#define AB_ORACLE_ISLAND_REPORTS_H

static const uint32_t *report_bodies;
static const uint32_t *report_contacts;
static size_t report_body_count;
static size_t report_contact_count;
static size_t report_record_count;
static uint32_t report_records[8][62];

static void record_island_report(void *listener, const void *contact,
                                 const void *impulse) {
    (void)listener;
    if (report_record_count >= 8) {
        fail("Too many native island reports\n");
    }
    size_t index = 0;
    while (index < report_contact_count &&
           report_contacts[index] != (uint32_t)(uintptr_t)contact) {
        ++index;
    }
    const uint32_t *values = impulse;
    if (index == report_contact_count || values[4] > 2) {
        fail("Invalid native island report\n");
    }
    uint32_t *record = report_records[report_record_count++];
    record[0] = (uint32_t)index;
    record[1] = values[4];
    for (size_t point = 0; point < 2; ++point) {
        record[2 + point] = point < values[4] ? values[point] : 0;
        record[4 + point] = point < values[4] ? values[2 + point] : 0;
    }
    for (size_t body = 0; body < report_body_count; ++body) {
        const uint32_t *current = (const uint32_t *)(uintptr_t)report_bodies[body];
        const uint32_t fields[] = {1, 36, 11, 12, 14, 16, 17, 18};
        for (size_t field = 0; field < 8; ++field) {
            record[6 + body * 8 + field] = current[fields[field]];
        }
    }
}

static void begin_island_reports(const uint32_t *bodies, size_t body_count,
                                 const uint32_t *contacts, size_t contact_count) {
    report_bodies = bodies;
    report_body_count = body_count;
    report_contacts = contacts;
    report_contact_count = contact_count;
    report_record_count = 0;
}

static void print_island_reports(void) {
    uint32_t count = (uint32_t)report_record_count;
    print_words(&count, 1);
    for (size_t report = 0; report < report_record_count; ++report) {
        print_words(report_records[report], 6 + report_body_count * 8);
    }
}

#endif
