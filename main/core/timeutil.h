// Calendar helpers that do not depend on the C library's time zone support, so
// results are identical on the ESP32 and on the host test build.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int year, month, day;     // month 1..12
    int hour, min, sec;
    int wday;                 // 0 = Sunday
} tu_fields_t;

int64_t tu_days_from_civil(int y, int m, int d);
int64_t tu_utc_from_fields(int y, int mo, int d, int h, int mi, int s);
void tu_fields_from_utc(int64_t t, tu_fields_t *f);

// "2026-10-25T10:32:14.000+0100", "...Z", "...+01:00" -> unix seconds.
bool tu_parse_iso8601(const char *s, int64_t *out);

// Europe/Vienna (CET/CEST, EU switching rules).
bool tu_vienna_is_dst(int64_t utc);
int tu_vienna_offset(int64_t utc);   // seconds east of UTC
void tu_vienna_fields(int64_t utc, tu_fields_t *f);
int64_t tu_vienna_to_utc(int y, int mo, int d, int h, int mi);

// "dd.mm.yyyy" + "HH:MM" (Vienna local) -> unix seconds, 0 on parse error.
int64_t tu_parse_vienna_date_time(const char *date, const char *hhmm);
