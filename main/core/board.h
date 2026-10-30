// Departure board data model shared by all providers, the UI and the web API.
// Portable C: no ESP-IDF dependencies, unit-tested on the host (see host/).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOARD_MAX_DEPS      64
#define BOARD_MAX_STOPS     4
#define DEP_LINE_LEN        12
#define DEP_TOWARDS_LEN     48
#define DEP_PLATFORM_LEN    8

typedef enum {
    VT_UNKNOWN = 0,
    VT_METRO,       // U-Bahn
    VT_TRAM,        // Strassenbahn
    VT_BUS,
    VT_NIGHTBUS,
    VT_SBAHN,
    VT_TRAIN,       // regional / long distance
} vehicle_type_t;

enum {
    DEP_F_BARRIER_FREE = 1 << 0,
    DEP_F_REALTIME     = 1 << 1,
    DEP_F_CANCELLED    = 1 << 2,
    DEP_F_AIRPORT      = 1 << 3,
};

typedef struct {
    char line[DEP_LINE_LEN];          // "U3", "52", "S 50", "REX 51"
    char towards[DEP_TOWARDS_LEN];    // UTF-8 destination
    char platform[DEP_PLATFORM_LEN];  // track / platform, may be empty
    char dir;                         // Wiener Linien direction 'H'/'R', 0 if unknown
    int64_t planned;                  // unix UTC seconds, 0 if unknown
    int64_t expected;                 // unix UTC seconds from realtime data, 0 if none
    int16_t countdown;                // minutes as reported at fetch time, -1 if unknown
    uint8_t type;                     // vehicle_type_t
    uint8_t flags;                    // DEP_F_*
    uint8_t stop;                     // index of the configured stop it belongs to
} departure_t;

typedef struct {
    departure_t dep[BOARD_MAX_DEPS];
    int count;
    char title[48];         // station name(s) shown in the header
    char notice[160];       // traffic info shown as a ticker, may be empty
    int64_t fetched_at;     // unix time the data refers to (0 if unknown)
} board_t;

// One rendered row: a departure plus, when grouping by line, the minutes of the
// following departure of the same line/direction (-1 if there is none) and, if
// `dep` is not barrier-free, the next one of the line/direction that is (or NULL).
typedef struct {
    const departure_t *dep;
    int next_minutes;
    const departure_t *step_free;
} board_row_t;

void board_clear(board_t *b);
departure_t *board_add(board_t *b);          // NULL when full
void board_append(board_t *dst, const board_t *src);
// Sorts `src` and appends its `max` soonest departures (so every stop gets a
// fair share of the board); takes over src's notice if dst has none.
void board_append_soonest(board_t *dst, board_t *src, int max);
// Copies the departures of one stop from `src` (used to keep the last good
// data of a stop whose fetch failed). Returns the number copied.
int board_copy_stop(board_t *dst, const board_t *src, uint8_t stop);
void board_sort(board_t *b);                 // by effective departure time

int64_t dep_time(const departure_t *d);      // expected if known, else planned
bool dep_is_delayed(const departure_t *d);   // at least one minute late
// Whole minutes until departure (never negative). Falls back to the countdown
// reported by the source when there is no clock (now <= 0) or no timestamp.
int dep_minutes(const departure_t *d, int64_t now);

// Builds display rows: drops departures already gone or closer than the walk
// time of their stop (walk_min may be NULL), optionally groups by line+towards.
int board_rows(const board_t *b, int64_t now, bool group, const uint8_t *walk_min,
               board_row_t *rows, int max_rows);

// Orders line names as people read them: digit runs by value ("2" < "18" <
// "D" < "N25" < "U3"). strcmp-style result.
int board_line_cmp(const char *a, const char *b);
// Sorts rows by line name, keeping the order of rows with the same line.
void board_rows_by_line(board_row_t *rows, int n);
