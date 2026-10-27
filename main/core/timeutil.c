#include "timeutil.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Howard Hinnant's days_from_civil / civil_from_days algorithms.
int64_t tu_days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

int64_t tu_utc_from_fields(int y, int mo, int d, int h, int mi, int s)
{
    return tu_days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

void tu_fields_from_utc(int64_t t, tu_fields_t *f)
{
    int64_t days = t / 86400;
    int64_t secs = t % 86400;
    if (secs < 0) {
        secs += 86400;
        days--;
    }
    f->hour = (int)(secs / 3600);
    f->min = (int)(secs / 60 % 60);
    f->sec = (int)(secs % 60);
    f->wday = (int)((days % 7 + 11) % 7);  // 1970-01-01 was a Thursday

    int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    f->day = (int)(doy - (153 * mp + 2) / 5 + 1);
    f->month = (int)(mp < 10 ? mp + 3 : mp - 9);
    f->year = (int)(yoe + era * 400 + (f->month <= 2));
}

static int read_int(const char **p, int digits)
{
    int v = 0;
    for (int i = 0; i < digits; i++) {
        if (!isdigit((unsigned char)**p)) {
            return -1;
        }
        v = v * 10 + (**p - '0');
        (*p)++;
    }
    return v;
}

bool tu_parse_iso8601(const char *s, int64_t *out)
{
    if (!s) {
        return false;
    }
    const char *p = s;
    int y = read_int(&p, 4);
    if (y < 0 || *p++ != '-') return false;
    int mo = read_int(&p, 2);
    if (mo < 1 || *p++ != '-') return false;
    int d = read_int(&p, 2);
    if (d < 1 || (*p != 'T' && *p != ' ')) return false;
    p++;
    int h = read_int(&p, 2);
    if (h < 0 || *p++ != ':') return false;
    int mi = read_int(&p, 2);
    if (mi < 0) return false;
    int sec = 0;
    if (*p == ':') {
        p++;
        sec = read_int(&p, 2);
        if (sec < 0) return false;
    }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) p++;
    }
    int offset = 0;
    if (*p == '+' || *p == '-') {
        int sign = *p == '-' ? -1 : 1;
        p++;
        int oh = read_int(&p, 2);
        if (oh < 0) return false;
        if (*p == ':') p++;
        int om = read_int(&p, 2);
        if (om < 0) return false;
        offset = sign * (oh * 3600 + om * 60);
    } else if (*p == 'Z') {
        p++;
    }
    *out = tu_utc_from_fields(y, mo, d, h, mi, sec) - offset;
    return true;
}

// EU rule: summer time from the last Sunday of March 01:00 UTC until the last
// Sunday of October 01:00 UTC.
static int64_t last_sunday_0100_utc(int year, int month)
{
    int64_t days = tu_days_from_civil(year, month, 31);
    int wday = (int)((days % 7 + 11) % 7);
    return (days - wday) * 86400 + 3600;
}

bool tu_vienna_is_dst(int64_t utc)
{
    tu_fields_t f;
    tu_fields_from_utc(utc, &f);
    return utc >= last_sunday_0100_utc(f.year, 3) && utc < last_sunday_0100_utc(f.year, 10);
}

int tu_vienna_offset(int64_t utc)
{
    return tu_vienna_is_dst(utc) ? 7200 : 3600;
}

void tu_vienna_fields(int64_t utc, tu_fields_t *f)
{
    tu_fields_from_utc(utc + tu_vienna_offset(utc), f);
}

int64_t tu_vienna_to_utc(int y, int mo, int d, int h, int mi)
{
    int64_t local = tu_utc_from_fields(y, mo, d, h, mi, 0);
    // Try summer time first; in the repeated autumn hour this picks CEST.
    int64_t utc = local - 7200;
    if (!tu_vienna_is_dst(utc)) {
        utc = local - 3600;
    }
    return utc;
}

int64_t tu_parse_vienna_date_time(const char *date, const char *hhmm)
{
    int d = 0, mo = 0, y = 0, h = 0, mi = 0;
    if (!date || !hhmm || sscanf(date, "%d.%d.%d", &d, &mo, &y) != 3 ||
        sscanf(hhmm, "%d:%d", &h, &mi) != 2) {
        return 0;
    }
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) {
        return 0;
    }
    return tu_vienna_to_utc(y, mo, d, h, mi);
}
