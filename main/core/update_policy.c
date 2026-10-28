#include "update_policy.h"

#include <ctype.h>

#include "timeutil.h"

bool upd_parse_version(const char *s, bool strict, int v[3])
{
    if (!s) {
        return false;
    }
    if (*s == 'v') {
        s++;
    }
    for (int i = 0; i < 3; i++) {
        if (!isdigit((unsigned char)*s)) {
            return false;
        }
        long n = 0;
        while (isdigit((unsigned char)*s)) {
            n = n * 10 + (*s++ - '0');
            if (n > 99999) {
                return false;
            }
        }
        v[i] = (int)n;
        if (i < 2 && *s++ != '.') {
            return false;
        }
    }
    return !strict || *s == 0;
}

bool upd_is_newer(const char *candidate, const char *running)
{
    int c[3], r[3];
    if (!upd_parse_version(candidate, true, c) || !upd_parse_version(running, false, r)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (c[i] != r[i]) {
            return c[i] > r[i];
        }
    }
    return false;
}

int64_t upd_next_check(int64_t now, uint32_t jitter_s)
{
    tu_fields_t f;
    tu_vienna_fields(now, &f);
    int64_t t = tu_vienna_to_utc(f.year, f.month, f.day, UPD_CHECK_HOUR, 0);
    if (t <= now) {
        // Tomorrow's date, via the day number (local time does not matter here).
        tu_fields_t d;
        tu_fields_from_utc((tu_days_from_civil(f.year, f.month, f.day) + 1) * 86400, &d);
        t = tu_vienna_to_utc(d.year, d.month, d.day, UPD_CHECK_HOUR, 0);
    }
    return t + jitter_s % UPD_JITTER_S;
}
