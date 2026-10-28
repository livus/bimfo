// When the device looks for a new firmware and which versions it accepts.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Automatic checks start at this Vienna local hour, plus a random delay.
#define UPD_CHECK_HOUR      3
#define UPD_JITTER_S        3600

// "1.2.3" or "v1.2.3" -> true. With `strict`, anything after the third number
// (a pre-release or development suffix like "1.2.3-rc1") is refused; without,
// it is ignored.
bool upd_parse_version(const char *s, bool strict, int v[3]);

// True if `candidate` is a release version newer than `running`.
bool upd_is_newer(const char *candidate, const char *running);

// The next automatic check after unix time `now`: the next UPD_CHECK_HOUR:00
// Vienna local time strictly after `now`, plus `jitter_s` (0..UPD_JITTER_S-1).
int64_t upd_next_check(int64_t now, uint32_t jitter_s);
