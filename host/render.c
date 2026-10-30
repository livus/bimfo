// Renders every screen of the UI with real (captured) and synthetic data into
// host/out/*.png plus an index.html gallery. Uses the exact UI code and LVGL
// version of the firmware; only the display driver is replaced by a framebuffer.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "board.h"
#include "fixture.h"
#include "lvgl.h"
#include "oebb_parse.h"
#include "png.h"
#include "timeutil.h"
#include "ui.h"
#include "wl_parse.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static lv_color_t s_fb[UI_W * UI_H];
static FILE *s_index;
static int s_failures;
static uint32_t s_mem_peak;   // LVGL heap in use at its fullest (bytes)

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++) {
        for (int x = a->x1; x <= a->x2; x++) {
            s_fb[y * UI_W + x] = *px++;
        }
    }
    lv_disp_flush_ready(drv);
}

static void save(const char *name, const char *caption)
{
    static uint8_t rgb[UI_W * UI_H * 3];
    for (int i = 0; i < UI_W * UI_H; i++) {
        lv_color_t c = s_fb[i];
        // RGB565 -> RGB888 the way the panel shows it.
        rgb[i * 3 + 0] = (uint8_t)(c.ch.red << 3 | c.ch.red >> 2);
        rgb[i * 3 + 1] = (uint8_t)(c.ch.green << 2 | c.ch.green >> 4);
        rgb[i * 3 + 2] = (uint8_t)(c.ch.blue << 3 | c.ch.blue >> 2);
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.png", OUT_DIR, name);
    if (!png_write_rgb(path, rgb, UI_W, UI_H)) {
        fprintf(stderr, "cannot write %s\n", path);
        s_failures++;
    }
    // 3x nearest-neighbour copy for looking at single pixels.
    const int k = 3;
    uint8_t *big = malloc((size_t)UI_W * k * UI_H * k * 3);
    for (int y = 0; y < UI_H * k; y++) {
        for (int x = 0; x < UI_W * k; x++) {
            memcpy(big + ((size_t)y * UI_W * k + x) * 3, rgb + ((size_t)(y / k) * UI_W + x / k) * 3, 3);
        }
    }
    snprintf(path, sizeof(path), "%s/%s@3x.png", OUT_DIR, name);
    png_write_rgb(path, big, UI_W * k, UI_H * k);
    free(big);
    fprintf(s_index, "<figure><img src=\"%s@3x.png\" width=\"%d\"><figcaption><b>%s</b> %s</figcaption></figure>\n",
            name, UI_W * 2, name, caption);
    printf("rendered %-28s %s\n", name, caption);
}

static void render(const char *name, const char *caption, const ui_state_t *st, const board_t *b, int64_t now)
{
    ui_update(st, b, now);
    lv_refr_now(NULL);
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    if (mon.total_size - mon.free_size > s_mem_peak) s_mem_peak = mon.total_size - mon.free_size;
    save(name, caption);
}

// Lets LVGL time pass (animations), in steps like the display task.
static void advance(uint32_t ms)
{
    while (ms > 0) {
        uint32_t step = ms < 20 ? ms : 20;
        lv_tick_inc(step);
        lv_timer_handler();
        ms -= step;
    }
}

static ui_state_t state(ui_screen_t screen, uint8_t theme)
{
    ui_state_t st;
    memset(&st, 0, sizeof(st));
    st.screen = screen;
    st.theme = theme;
    st.group_lines = true;
    st.wl_paging = true;     // the defaults of the configuration
    st.wl_step_free = true;
    strcpy(st.url, "http://bimfo.local");
    return st;
}

static departure_t *synth(board_t *b, int64_t now, const char *line, const char *towards, int minutes,
                          uint8_t type, uint8_t flags)
{
    departure_t *d = board_add(b);
    strcpy(d->line, line);
    strcpy(d->towards, towards);
    d->planned = now + minutes * 60 + 20;
    d->expected = (flags & DEP_F_REALTIME) ? d->planned : 0;
    d->countdown = (int16_t)minutes;
    d->type = type;
    d->flags = flags;
    return d;
}

static int64_t load_wl(board_t *b, const char *fixture, const cfg_stop_t *stop, const char *title)
{
    char *json = fixture_load(fixture, NULL);
    int64_t server_time = 0;
    board_clear(b);
    if (wl_parse_monitor(json, stop, 0, b, &server_time, NULL) != WL_OK) {
        fprintf(stderr, "parse %s failed\n", fixture);
        s_failures++;
    }
    board_sort(b);
    strcpy(b->title, title);
    b->fetched_at = server_time;
    free(json);
    return server_time;
}

static void load_oebb(board_t *b, const char *fixture, int64_t now)
{
    char *txt = fixture_load(fixture, NULL);
    board_clear(b);
    if (oebb_parse_board(txt, NULL, 0, b, b->title, sizeof(b->title)) < 0) {
        fprintf(stderr, "parse %s failed\n", fixture);
        s_failures++;
    }
    board_sort(b);
    b->fetched_at = now;
    free(txt);
}

int main(void)
{
    MKDIR(OUT_DIR);
    char path[512];
    snprintf(path, sizeof(path), "%s/index.html", OUT_DIR);
    s_index = fopen(path, "w");
    fprintf(s_index, "<!doctype html><meta charset=utf-8><title>Bimfo renders</title>"
                     "<style>body{background:#222;color:#ddd;font:14px sans-serif}figure{display:inline-block;"
                     "margin:8px}img{image-rendering:pixelated;border:1px solid #555}</style>\n");

    lv_init();
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[UI_W * UI_H];
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, UI_W * UI_H);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = UI_W;
    drv.ver_res = UI_H;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &draw_buf;
    drv.full_refresh = 1;
    lv_disp_drv_register(&drv);
    ui_init();

    static board_t b;
    ui_state_t st;

    // --- boot / setup -----------------------------------------------------
    for (int t = 0; t < THEME_COUNT; t++) {
        const char *th = cfg_theme_name((uint8_t)t);
        char name[64];
        st = state(UI_SCREEN_BOOT, (uint8_t)t);
        strcpy(st.status, "Verbinde mit WLAN\n\xE2\x80\x9EZuhause\xE2\x80\x9C ...");
        snprintf(name, sizeof(name), "%s_boot", th);
        render(name, "boot, connecting", &st, NULL, 0);
        if (t != THEME_OEBB) {
            // The tram of the LED styles: pulling away, then the next one arriving.
            advance(3500);
            snprintf(name, sizeof(name), "%s_boot_out", th);
            render(name, "1 s after pulling away", &st, NULL, 0);
            advance(4000);
            snprintf(name, sizeof(name), "%s_boot_in", th);
            render(name, "the next tram arriving", &st, NULL, 0);
        }

        st = state(UI_SCREEN_SETUP, (uint8_t)t);
        strcpy(st.ap_ssid, "Bimfo-1A2B");
        strcpy(st.url, "http://192.168.4.1");
        if (t == THEME_OEBB) {
            strcpy(st.ap_pass, "fahrplan");
            strcpy(st.status, "WLAN \xE2\x80\x9EZuhause\xE2\x80\x9C nicht erreichbar");
        }
        snprintf(name, sizeof(name), "%s_setup", th);
        render(name, "access point + QR code", &st, NULL, 0);
    }

    // --- Wiener Linien data, WL theme ------------------------------------
    cfg_stop_t all = {.kind = STOP_WL, .id = 60201468};
    int64_t wl_now = load_wl(&b, "wl_westbahnhof_diva.json", &all, "Westbahnhof");
    st = state(UI_SCREEN_BOARD, THEME_WL);
    render("wl_grouped", "Westbahnhof, next departure per line and direction", &st, &b, wl_now);
    render("wl_grouped_blink", "same, one second later (the square jumps)", &st, &b, wl_now + 1);
    render("wl_page2", "ten seconds later: the next page of lines", &st, &b, wl_now + 10);
    st.group_lines = false;
    render("wl_list", "grouping off: the same, the WL style always groups", &st, &b, wl_now);

    cfg_stop_t sel = {.kind = STOP_WL, .id = 60201468, .nsel = 3,
                      .sel = {{"U3", 'H', 4921}, {"U3", 'R', 4920}, {"52", 'H', 464}}};
    load_wl(&b, "wl_westbahnhof_diva.json", &sel, "Westbahnhof U3 + 52");
    st = state(UI_SCREEN_BOARD, THEME_WL);
    render("wl_selected", "only U3 both directions + 52 Baumgarten, by line (paging on)", &st, &b, wl_now);
    st.wl_paging = false;
    render("wl_selected_by_time", "same with paging off: next departure first", &st, &b, wl_now);
    st.wl_paging = true;
    st.walk_min[0] = 4;
    render("wl_walk4", "same with 4 min walking time", &st, &b, wl_now);
    st.walk_min[0] = 0;

    load_wl(&b, "wl_westbahnhof_diva.json", &all, "Westbahnhof");
    strcpy(b.notice, "U6: Unregelm\xC3\xA4\xC3\x9Fige Intervalle wegen eines Fahrzeugschadens");
    render("wl_notice", "traffic info: not shown by default, like the real displays", &st, &b, wl_now);
    st.wl_notice = true;
    render("wl_notice_on", "traffic info ticker (setting on)", &st, &b, wl_now);
    st.wl_notice = false;
    b.notice[0] = 0;
    strcpy(st.status, "Offline - Stand 10:31");
    render("wl_offline", "offline: no warning, like the real displays", &st, &b, wl_now);
    st.status[0] = 0;

    load_wl(&b, "wl_johnstrasse_4925.json", NULL, "Johnstra\xC3\x9F" "e");
    render("wl_johnstrasse", "single U3 platform", &st, &b, wl_now);

    board_clear(&b);
    render("wl_loading", "no data yet: \"Fahrplanaushang beachten\"", &st, &b, wl_now);
    b.fetched_at = wl_now;
    strcpy(b.title, "Westbahnhof");
    render("wl_empty", "fetched, nothing departs", &st, &b, wl_now);
    st.no_stops = true;
    render("wl_nostops", "nothing configured", &st, &b, wl_now);
    st.no_stops = false;

    // Synthetic edge cases: umlauts, long names, due now, night lines, 99+.
    board_clear(&b);
    strcpy(b.title, "Gumpendorfer Stra\xC3\x9F" "e / Mariahilfer G\xC3\xBCrtel");
    b.fetched_at = wl_now;
    synth(&b, wl_now, "6", "Geiereckstra\xC3\x9F" "e", 0, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, wl_now, "N25", "Gro\xC3\x9F" "feldsiedlung", 1, VT_NIGHTBUS, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, wl_now, "U2Z", "Seestadt \xC3\x9C" "ber Sch\xC3\xB6" "nbrunner Schlo\xC3\x9F" "stra\xC3\x9F" "e", 7,
          VT_METRO, DEP_F_REALTIME);
    synth(&b, wl_now, "13A", "H\xC3\xB6" "chst\xC3\xA4" "dtplatz", 12, VT_BUS, 0);
    synth(&b, wl_now, "VAB 1", "Flughafen Wien", 145, VT_BUS, 0);
    st = state(UI_SCREEN_BOARD, THEME_WL);
    st.group_lines = false;
    st.wl_paging = false;   // the first three lines
    render("wl_edge", "umlauts, long names (shortened, then scrolling), due now, >99 min", &st, &b, wl_now);
    st.wl_paging = true;
    b.count = 0;
    synth(&b, wl_now, "13A", "H\xC3\xB6" "chst\xC3\xA4" "dtplatz", 3, VT_BUS, DEP_F_BARRIER_FREE);
    synth(&b, wl_now, "VAB 1", "Flughafen Wien", 9, VT_BUS, 0);
    synth(&b, wl_now, "U2Z", "Seestadt", 12, VT_METRO, DEP_F_REALTIME);
    render("wl_edge2", "13A small A, scheduled times with a leading zero, VAB 1 in two rows", &st, &b, wl_now);

    // As in the photos (docs/research/wl_display): wrapping and U-/S-Bahn symbols.
    board_clear(&b);
    b.fetched_at = wl_now;
    synth(&b, wl_now, "1", "Stefan-Fadinger-Platz", 1, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, wl_now, "D", "Hauptbahnhof Ost S", 17, VT_TRAM, DEP_F_REALTIME);
    synth(&b, wl_now, "71", "Zentralfriedhof 2. Tor", 24, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    st.group_lines = true;
    render("wl_burgtheater", "Burgtheater photo: wrapping, S-Bahn symbol", &st, &b, wl_now);
    board_clear(&b);
    b.fetched_at = wl_now;
    synth(&b, wl_now, "49", "Ring, U Volkstheater", 7, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, wl_now, "5", "Praterstern S U", 8, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, wl_now, "48A", "Ring, U Volkstheater", 4, VT_BUS, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    board_sort(&b);
    render("wl_symbols", "U- and S-Bahn symbols, 48A without wheelchair", &st, &b, wl_now);

    // Next tram not low-floor: it takes turns (20 s each) with the next one that is.
    int64_t t = wl_now - wl_now % 40;
    board_clear(&b);
    b.fetched_at = t;
    synth(&b, t, "62", "Lainz, Wolkersbergenstra\xC3\x9F" "e", 2, VT_TRAM, DEP_F_REALTIME);
    synth(&b, t, "62", "Lainz, Wolkersbergenstra\xC3\x9F" "e", 9, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, t, "1", "Prater Hauptallee", 4, VT_TRAM, DEP_F_REALTIME | DEP_F_BARRIER_FREE);
    synth(&b, t, "D", "Nu\xC3\x9F" "dorf, Heiligenst\xC3\xA4" "dter Stra\xC3\x9F" "e / Zehenthofgasse", 6, VT_TRAM,
          DEP_F_REALTIME);
    render("wl_step_free", "62 not low-floor, D has no low-floor tram (and is shortened), 1 is", &st, &b, t);
    render("wl_step_free_alt", "20 s later: the low-floor 62 in 9 min, wheelchair blinking (on)", &st, &b, t + 20);
    render("wl_step_free_blink", "one second later: wheelchair off", &st, &b, t + 21);
    st.theme = THEME_COMPACT;
    render("compact_edge", "same in compact style", &st, &b, wl_now);

    // --- Wiener Linien data, compact theme --------------------------------
    load_wl(&b, "wl_westbahnhof_diva.json", &all, "Westbahnhof");
    st = state(UI_SCREEN_BOARD, THEME_COMPACT);
    render("compact_grouped", "Westbahnhof, all lines, grouped (next two)", &st, &b, wl_now);
    st.group_lines = false;
    render("compact_list", "Westbahnhof, list of departures", &st, &b, wl_now);
    strcpy(b.notice, "U6: Unregelm\xC3\xA4\xC3\x9Fige Intervalle wegen eines Fahrzeugschadens");
    strcpy(st.status, "Offline - Stand 10:31");
    render("compact_notice", "ticker and stale data warning", &st, &b, wl_now);
    st.no_stops = true;
    render("compact_nostops", "nothing configured", &st, &b, wl_now);

    // --- OEBB data, OEBB theme --------------------------------------------
    int64_t oebb_now = tu_vienna_to_utc(2026, 10, 25, 10, 30);
    load_oebb(&b, "oebb_westbahnhof_trains.txt", oebb_now);
    st = state(UI_SCREEN_BOARD, THEME_OEBB);
    render("oebb_trains", "Wien Westbahnhof, trains only", &st, &b, oebb_now);
    load_oebb(&b, "oebb_westbahnhof_all.txt", oebb_now);
    render("oebb_all", "Wien Westbahnhof, all products", &st, &b, oebb_now);

    board_clear(&b);
    strcpy(b.title, "Wien Hbf");
    b.fetched_at = oebb_now;
    departure_t *d = synth(&b, oebb_now, "RJX 162", "Z\xC3\xBCrich HB", 3, VT_TRAIN, DEP_F_REALTIME);
    strcpy(d->platform, "8");
    d->expected = d->planned + 12 * 60;
    d = synth(&b, oebb_now, "S 60", "Bruck/Leitha", 5, VT_SBAHN, DEP_F_REALTIME);
    strcpy(d->platform, "1");
    d = synth(&b, oebb_now, "REX 7", "Wolfsthal", 9, VT_TRAIN, DEP_F_REALTIME | DEP_F_CANCELLED);
    strcpy(d->platform, "2");
    d = synth(&b, oebb_now, "NJ 466", "M\xC3\xBCnchen Hbf \xC3\xBC" "ber Salzburg Hbf", 14, VT_TRAIN, DEP_F_REALTIME);
    strcpy(d->platform, "11A");
    d->expected = d->planned + 4 * 60;
    d = synth(&b, oebb_now, "CJX 9", "St. P\xC3\xB6lten Hbf", 21, VT_TRAIN, 0);
    strcpy(d->platform, "9");
    strcpy(b.notice, "Zwischen Wien Meidling und M\xC3\xB6" "dling Schienenersatzverkehr mit Bussen.");
    board_sort(&b);
    render("oebb_delays", "delays (yellow), cancellation, notice strip", &st, &b, oebb_now);
    b.notice[0] = 0;
    render("oebb_delays_nonotice", "same without notice: 5 rows", &st, &b, oebb_now);

    load_wl(&b, "wl_westbahnhof_diva.json", &all, "Westbahnhof");
    render("oebb_wl_data", "Wiener Linien data in OEBB style", &st, &b, wl_now);
    board_clear(&b);
    render("oebb_loading", "no data yet", &st, &b, wl_now);
    st.no_stops = true;
    render("oebb_nostops", "nothing configured", &st, &b, wl_now);
    st.no_stops = false;

    // --- Mixed providers merged on one board ------------------------------
    board_t wl, oe;
    load_wl(&wl, "wl_westbahnhof_diva.json", &sel, "");
    load_oebb(&oe, "oebb_westbahnhof_trains.txt", oebb_now);
    for (int i = 0; i < oe.count; i++) oe.dep[i].stop = 1;
    board_clear(&b);
    board_append_soonest(&b, &wl, BOARD_MAX_DEPS / 2);  // as the fetcher merges stops
    board_append_soonest(&b, &oe, BOARD_MAX_DEPS / 2);
    board_sort(&b);
    strcpy(b.title, "Westbahnhof (WL + \xC3\x96" "BB)");
    b.fetched_at = wl_now;
    // ÖBB fixture is from 10:30, WL from 10:32 - render at the OEBB time.
    st = state(UI_SCREEN_BOARD, THEME_WL);
    st.group_lines = false;
    render("mixed_wl", "WL + OEBB merged, WL style", &st, &b, oebb_now);
    st.theme = THEME_COMPACT;
    render("mixed_compact", "WL + OEBB merged, compact style", &st, &b, oebb_now);
    st.theme = THEME_OEBB;
    render("mixed_oebb", "WL + OEBB merged, OEBB style", &st, &b, oebb_now);

    // No clock yet: countdowns fall back to the reported values.
    load_wl(&b, "wl_westbahnhof_diva.json", &sel, "Westbahnhof (ohne Uhrzeit)");
    st = state(UI_SCREEN_BOARD, THEME_WL);
    render("wl_noclock", "clock unknown, countdown from feed", &st, &b, 0);

    // Soak: thousands of updates with changing data, themes and screens (as the
    // device does for months) must not leak LVGL memory.
    {
        static board_t boards[2];
        load_wl(&boards[0], "wl_westbahnhof_diva.json", &all, "Westbahnhof");
        load_oebb(&boards[1], "oebb_westbahnhof_all.txt", oebb_now);
        st = state(UI_SCREEN_BOARD, THEME_WL);
        ui_update(&st, &boards[0], wl_now);
        lv_refr_now(NULL);
        lv_mem_monitor_t base, mon;
        lv_mem_monitor(&base);
        for (int i = 0; i < 3000; i++) {
            st = state(i % 500 < 450 ? UI_SCREEN_BOARD : (i % 2 ? UI_SCREEN_SETUP : UI_SCREEN_BOOT),
                       (uint8_t)((i / 100) % THEME_COUNT));
            st.group_lines = (i / 37) % 2;
            strcpy(st.ap_ssid, "Bimfo-1A2B");
            if (i % 7 == 0) strcpy(st.status, "Offline - Stand 10:31");
            board_t *bd = &boards[(i / 50) % 2];
            strcpy(bd->notice, i % 300 < 20 ? "Störung auf der Linie U6" : "");
            ui_update(&st, bd, wl_now + i * 5);
            lv_timer_handler();
            if (i % 50 == 0) lv_refr_now(NULL);
        }
        st = state(UI_SCREEN_BOARD, THEME_WL);
        boards[0].notice[0] = 0;
        ui_update(&st, &boards[0], wl_now);
        lv_refr_now(NULL);
        lv_mem_monitor(&mon);
        printf("soak: LVGL free %u -> %u bytes, fragmentation %u%%\n", (unsigned)base.free_size,
               (unsigned)mon.free_size, (unsigned)mon.frag_pct);
        if (mon.free_size + 512 < base.free_size) {
            fprintf(stderr, "soak: LVGL memory leak (%d bytes)\n", (int)(base.free_size - mon.free_size));
            s_failures++;
        }
        if (mon.total_size - mon.free_size > s_mem_peak) s_mem_peak = mon.total_size - mon.free_size;
    }

    fclose(s_index);
    printf("LVGL heap peak %u of %u bytes\n", (unsigned)s_mem_peak, (unsigned)LV_MEM_SIZE);
    if (s_mem_peak > LV_MEM_SIZE * 3 / 4) {
        fprintf(stderr, "LVGL heap above 75%% - raise LV_MEM_SIZE (host/lv_conf.h and sdkconfig.defaults)\n");
        s_failures++;
    }
    printf("%s\n", s_failures ? "RENDER FAILURES" : "all renders written to " OUT_DIR);
    return s_failures ? 1 : 0;
}
