#include "board.h"

#include <stdlib.h>
#include <string.h>

void board_clear(board_t *b)
{
    memset(b, 0, sizeof(*b));
}

departure_t *board_add(board_t *b)
{
    if (b->count >= BOARD_MAX_DEPS) {
        return NULL;
    }
    departure_t *d = &b->dep[b->count++];
    memset(d, 0, sizeof(*d));
    d->countdown = -1;
    return d;
}

void board_append(board_t *dst, const board_t *src)
{
    for (int i = 0; i < src->count; i++) {
        departure_t *d = board_add(dst);
        if (!d) {
            break;
        }
        *d = src->dep[i];
    }
}

void board_append_soonest(board_t *dst, board_t *src, int max)
{
    board_sort(src);
    for (int i = 0; i < src->count && i < max; i++) {
        departure_t *d = board_add(dst);
        if (!d) {
            break;
        }
        *d = src->dep[i];
    }
    if (!dst->notice[0] && src->notice[0]) {
        memcpy(dst->notice, src->notice, sizeof(dst->notice));
    }
}

int board_copy_stop(board_t *dst, const board_t *src, uint8_t stop)
{
    int n = 0;
    for (int i = 0; i < src->count; i++) {
        if (src->dep[i].stop != stop) {
            continue;
        }
        departure_t *d = board_add(dst);
        if (!d) {
            break;
        }
        *d = src->dep[i];
        n++;
    }
    return n;
}

int64_t dep_time(const departure_t *d)
{
    return d->expected ? d->expected : d->planned;
}

bool dep_is_delayed(const departure_t *d)
{
    return d->expected && d->planned && d->expected - d->planned >= 60;
}

int dep_minutes(const departure_t *d, int64_t now)
{
    int64_t t = dep_time(d);
    if (now <= 0 || t == 0) {
        return d->countdown < 0 ? 0 : d->countdown;
    }
    int64_t diff = t - now;
    if (diff <= 0) {
        return 0;
    }
    return (int)(diff / 60);
}

static int cmp_dep(const void *a, const void *b)
{
    const departure_t *da = a, *db = b;
    int64_t ta = dep_time(da), tb = dep_time(db);
    if (ta && tb && ta != tb) {
        return ta < tb ? -1 : 1;
    }
    if (da->countdown != db->countdown) {
        return da->countdown - db->countdown;
    }
    return strcmp(da->line, db->line);
}

void board_sort(board_t *b)
{
    qsort(b->dep, b->count, sizeof(b->dep[0]), cmp_dep);
}

// A departure that left more than a minute ago (clock known) is hidden.
static bool dep_gone(const departure_t *d, int64_t now)
{
    int64_t t = dep_time(d);
    return now > 0 && t && t < now - 30;
}

int board_rows(const board_t *b, int64_t now, bool group, const uint8_t *walk_min,
               board_row_t *rows, int max_rows)
{
    int n = 0;
    // When grouping, keep scanning after the rows are full: later departures
    // still provide the "next" time of rows already shown.
    for (int i = 0; i < b->count && (group || n < max_rows); i++) {
        const departure_t *d = &b->dep[i];
        if (dep_gone(d, now)) {
            continue;
        }
        if (walk_min && d->stop < BOARD_MAX_STOPS && dep_minutes(d, now) < walk_min[d->stop]) {
            continue;
        }
        if (group) {
            // Grouped rows show the next catchable departures; cancelled trains
            // only stay visible in list mode, where they are marked as such.
            if (d->flags & DEP_F_CANCELLED) {
                continue;
            }
            bool merged = false;
            for (int r = 0; r < n; r++) {
                const departure_t *o = rows[r].dep;
                if (strcmp(o->line, d->line) == 0 && strcmp(o->towards, d->towards) == 0) {
                    if (rows[r].next_minutes < 0) {
                        rows[r].next_minutes = dep_minutes(d, now);
                    }
                    merged = true;
                    break;
                }
            }
            if (merged || n >= max_rows) {
                continue;
            }
        }
        rows[n].dep = d;
        rows[n].next_minutes = -1;
        n++;
    }
    return n;
}
