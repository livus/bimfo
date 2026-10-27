// Robustness test: feeds truncated and randomly corrupted versions of every
// captured payload to the parsers (as a flaky connection or a changed API
// would) and checks that they never crash or overrun the board.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "board.h"
#include "dns_reply.h"
#include "fixture.h"
#include "oebb_parse.h"
#include "wl_parse.h"

static uint32_t s_rng = 12345;

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return s_rng >> 8;
}

static int s_runs, s_bad;

static void check_board(const board_t *b, const char *what)
{
    if (b->count < 0 || b->count > BOARD_MAX_DEPS) {
        fprintf(stderr, "%s: board count %d out of range\n", what, b->count);
        s_bad++;
    }
    for (int i = 0; i < b->count; i++) {
        const departure_t *d = &b->dep[i];
        if (memchr(d->line, 0, sizeof(d->line)) == NULL || memchr(d->towards, 0, sizeof(d->towards)) == NULL ||
            memchr(d->platform, 0, sizeof(d->platform)) == NULL) {
            fprintf(stderr, "%s: unterminated string\n", what);
            s_bad++;
        }
    }
    if (memchr(b->notice, 0, sizeof(b->notice)) == NULL) {
        fprintf(stderr, "%s: unterminated notice\n", what);
        s_bad++;
    }
}

static void run_all(const char *payload)
{
    static board_t b;
    cfg_stop_t stop = {.nsel = 1, .sel = {{"U3", 'H', 4921}}};

    board_clear(&b);
    wl_parse_monitor(payload, NULL, 0, &b, NULL, NULL);
    check_board(&b, "wl");
    board_clear(&b);
    wl_parse_monitor(payload, &stop, 0, &b, NULL, NULL);
    check_board(&b, "wl filtered");

    wl_line_info_t lines[16];
    wl_list_lines(payload, lines, 16);

    board_clear(&b);
    char station[48];
    oebb_parse_board(payload, NULL, 0, &b, station, sizeof(station));
    check_board(&b, "oebb");
    oebb_station_t st[4];
    oebb_parse_search(payload, st, 4);

    wl_station_t ws[4];
    wl_search_stations(payload, "west", ws, 4);

    app_config_t c;
    cfg_defaults(&c);
    char err[32];
    if (cfg_from_json(&c, payload, err, sizeof(err))) {
        char *j = cfg_to_json(&c, true);
        free(j);
    }
    s_runs++;
}

int main(void)
{
    static const char *const fixtures[] = {
        "wl_westbahnhof_diva.json", "wl_johnstrasse_4925.json", "oebb_westbahnhof_all.txt",
        "oebb_westbahnhof_trains.txt", "oebb_getstop_westbahnhof.txt", "wl_haltestellen.csv",
    };
    for (size_t f = 0; f < sizeof(fixtures) / sizeof(fixtures[0]); f++) {
        size_t len;
        char *orig = fixture_load(fixtures[f], &len);
        char *buf = malloc(len + 1);

        // Every truncation point near the start and end, sampled in between.
        for (size_t cut = 0; cut <= len; cut += (cut < 200 || len - cut < 200) ? 1 : 97) {
            memcpy(buf, orig, cut);
            buf[cut] = 0;
            run_all(buf);
        }
        // Random byte corruption.
        for (int i = 0; i < 300; i++) {
            memcpy(buf, orig, len + 1);
            int flips = 1 + (int)(rnd() % 8);
            for (int k = 0; k < flips; k++) {
                static const char evil[] = "{}[]\":,\\0123456789eE.-+ \xC3\xFF";
                buf[rnd() % len] = evil[rnd() % (sizeof(evil) - 1)];
            }
            run_all(buf);
        }
        free(buf);
        free(orig);
    }
    // Pathological inputs.
    run_all("");
    run_all("{");
    run_all("null");
    run_all("{\"data\":{\"monitors\":{}},\"message\":{\"messageCode\":1}}");
    run_all("{\"data\":{\"monitors\":[{\"lines\":[{\"name\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
            "\"towards\":\"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\","
            "\"departures\":{\"departure\":[{\"departureTime\":{\"timePlanned\":\"9999-99-99T99:99:99\"}}]}}]}]},"
            "\"message\":{\"messageCode\":1}}");
    // Deep nesting must not blow the stack.
    char *deep = malloc(20001);
    memset(deep, '[', 20000);
    deep[20000] = 0;
    run_all(deep);
    free(deep);

    // DNS replies built from random packets must stay within the buffer.
    for (int i = 0; i < 200000; i++) {
        uint8_t pkt[96 + 16];
        int len = (int)(rnd() % 96);
        for (int k = 0; k < len; k++) pkt[k] = (uint8_t)rnd();
        if ((i & 1) && len > 5) {
            pkt[2] &= 0x7F;  // make every other packet a plausible single query
            pkt[4] = 0;
            pkt[5] = 1;
        }
        memset(pkt + 96, 0xAB, 16);  // canary behind the declared capacity
        int r = dns_build_reply(pkt, len, 96, 0x0104A8C0);
        if (r < 0 || r > 96 || pkt[96] != 0xAB) {
            fprintf(stderr, "dns: bad reply length %d\n", r);
            s_bad++;
        }
        s_runs++;
    }

    printf("%d fuzz runs, %d problems\n", s_runs, s_bad);
    return s_bad ? 1 : 0;
}
