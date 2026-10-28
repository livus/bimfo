// Unit tests for the device-independent code, run against API responses that
// were captured from the live services (host/fixtures).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_json.h"
#include "app_config.h"
#include "dns_reply.h"
#include "board.h"
#include "fixture.h"
#include "oebb_parse.h"
#include "textutil.h"
#include "timeutil.h"
#include "update_policy.h"
#include "wl_parse.h"

static int s_checks, s_failed;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        s_checks++;                                                                 \
        if (!(cond)) {                                                              \
            s_failed++;                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                           \
    } while (0)
#define CHECK_STR(a, b)                                                                         \
    do {                                                                                        \
        s_checks++;                                                                             \
        if (strcmp((a), (b)) != 0) {                                                            \
            s_failed++;                                                                         \
            fprintf(stderr, "%s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b));        \
        }                                                                                       \
    } while (0)
#define CHECK_INT(a, b)                                                                         \
    do {                                                                                        \
        s_checks++;                                                                             \
        long long _a = (long long)(a), _b = (long long)(b);                                     \
        if (_a != _b) {                                                                         \
            s_failed++;                                                                         \
            fprintf(stderr, "%s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, _a, _b); \
        }                                                                                       \
    } while (0)

static void test_time(void)
{
    CHECK_INT(tu_days_from_civil(1970, 1, 1), 0);
    CHECK_INT(tu_days_from_civil(2000, 3, 1), 11017);
    CHECK_INT(tu_utc_from_fields(2026, 10, 25, 9, 32, 14), 1792920734);

    int64_t t;
    CHECK(tu_parse_iso8601("2026-10-25T10:32:14.000+0100", &t));
    CHECK_INT(t, 1792920734);
    CHECK(tu_parse_iso8601("2026-10-25T09:32:14Z", &t));
    CHECK_INT(t, 1792920734);
    CHECK(tu_parse_iso8601("2026-10-25T11:32:14+02:00", &t));
    CHECK_INT(t, 1792920734);
    CHECK(!tu_parse_iso8601("", &t));
    CHECK(!tu_parse_iso8601(NULL, &t));
    CHECK(!tu_parse_iso8601("2026-10-25", &t));
    CHECK(!tu_parse_iso8601("garbage", &t));

    // EU switching: 2026-03-29 01:00 UTC and 2026-10-25 01:00 UTC (today!).
    CHECK(!tu_vienna_is_dst(tu_utc_from_fields(2026, 3, 29, 0, 59, 59)));
    CHECK(tu_vienna_is_dst(tu_utc_from_fields(2026, 3, 29, 1, 0, 0)));
    CHECK(tu_vienna_is_dst(tu_utc_from_fields(2026, 10, 25, 0, 59, 59)));
    CHECK(!tu_vienna_is_dst(tu_utc_from_fields(2026, 10, 25, 1, 0, 0)));
    CHECK_INT(tu_vienna_offset(tu_utc_from_fields(2026, 7, 1, 12, 0, 0)), 7200);
    CHECK_INT(tu_vienna_offset(tu_utc_from_fields(2026, 1, 1, 12, 0, 0)), 3600);

    CHECK_INT(tu_vienna_to_utc(2026, 10, 25, 10, 30), tu_utc_from_fields(2026, 10, 25, 9, 30, 0));
    CHECK_INT(tu_vienna_to_utc(2026, 7, 1, 12, 0), tu_utc_from_fields(2026, 7, 1, 10, 0, 0));
    CHECK_INT(tu_vienna_to_utc(2026, 12, 31, 23, 59), tu_utc_from_fields(2026, 12, 31, 22, 59, 0));
    // Repeated hour in autumn resolves to summer time.
    CHECK_INT(tu_vienna_to_utc(2026, 10, 25, 2, 30), tu_utc_from_fields(2026, 10, 25, 0, 30, 0));

    tu_fields_t f;
    tu_vienna_fields(tu_utc_from_fields(2026, 10, 24, 23, 15, 7), &f);  // 01:15 CEST next day
    CHECK_INT(f.year, 2026);
    CHECK_INT(f.month, 10);
    CHECK_INT(f.day, 25);
    CHECK_INT(f.hour, 1);
    CHECK_INT(f.min, 15);
    CHECK_INT(f.sec, 7);
    CHECK_INT(f.wday, 0);

    // Round trip over a wide range of days, including leap years.
    for (int64_t day = -1000; day < 40000; day += 37) {
        int64_t ts = day * 86400 + 12345;
        tu_fields_from_utc(ts, &f);
        CHECK_INT(tu_utc_from_fields(f.year, f.month, f.day, f.hour, f.min, f.sec), ts);
    }

    CHECK_INT(tu_parse_vienna_date_time("25.10.2026", "10:53"), tu_utc_from_fields(2026, 10, 25, 9, 53, 0));
    CHECK_INT(tu_parse_vienna_date_time("25.10.2026", "xx"), 0);
    CHECK_INT(tu_parse_vienna_date_time(NULL, "10:00"), 0);
    CHECK_INT(tu_parse_vienna_date_time("45.10.2026", "10:00"), 0);
}

static void test_text(void)
{
    char s[64];
    strcpy(s, "St.P&#246;lten Hbf");
    tx_html_decode(s);
    CHECK_STR(s, "St.P\xC3\xB6lten Hbf");
    strcpy(s, "a&#8203;~&#8203;b &amp; c&lt;d&gt;");
    tx_html_decode(s);
    CHECK_STR(s, "a~b & c<d>");
    strcpy(s, "&#x;&#;&unknown; & &#xDF;");
    tx_html_decode(s);
    CHECK_STR(s, "&#x;&#;&unknown; & \xC3\x9F");

    char d[8];
    tx_copy(d, "Stra\xC3\x9F" "e", 6);  // would cut the 2-byte ß
    CHECK_STR(d, "Stra");
    tx_copy(d, "Stra\xC3\x9F" "e", 7);
    CHECK_STR(d, "Stra\xC3\x9F");
    tx_copy(d, NULL, sizeof(d));
    CHECK_STR(d, "");

    tx_fold("H\xC3\xB6" "chst\xC3\xA4" "dtplatz", s, sizeof(s));
    CHECK_STR(s, "hochstadtplatz");
    tx_fold("Gro\xC3\x9F" "e Sperlgasse", s, sizeof(s));
    CHECK_STR(s, "grosse sperlgasse");

    strcpy(s, "Wien+H%C3%BCtteldorf%2x%%41%00!");
    tx_url_decode(s);
    CHECK_STR(s, "Wien H\xC3\xBCtteldorf%2x%A!");

    strcpy(s, "  x y \r\n");
    tx_trim(s);
    CHECK_STR(s, "x y");
    strcpy(s, "Ybbs Bahnhof");
    CHECK(tx_strip_suffix(s, " Bahnhof"));
    CHECK_STR(s, "Ybbs");
}

static const departure_t *find(const board_t *b, const char *line, char dir)
{
    for (int i = 0; i < b->count; i++) {
        if (strcmp(b->dep[i].line, line) == 0 && (!dir || b->dep[i].dir == dir)) return &b->dep[i];
    }
    return NULL;
}

static void test_wl(void)
{
    char *json = fixture_load("wl_westbahnhof_diva.json", NULL);
    board_t b;
    board_clear(&b);
    int64_t server = 0;
    int code = 0;
    CHECK_INT(wl_parse_monitor(json, NULL, 2, &b, &server, &code), WL_OK);
    CHECK_INT(code, 1);
    CHECK_INT(server, tu_utc_from_fields(2026, 10, 25, 9, 32, 14));
    // 13 line entries with ~10 departures each: the next WL_MAX_PER_LINE of
    // each are kept, so the lines listed last (U6) are not crowded out.
    CHECK_INT(b.count, 13 * WL_MAX_PER_LINE);
    CHECK(find(&b, "U6", 'H') && find(&b, "U6", 'R'));
    const departure_t *u3 = find(&b, "U3", 'H');
    CHECK(u3 != NULL);
    if (u3) {
        CHECK_STR(u3->towards, "Simmering");
        CHECK_INT(u3->type, VT_METRO);
        CHECK_INT(u3->stop, 2);
        CHECK(u3->flags & DEP_F_REALTIME);
        CHECK(u3->planned > 0 && u3->expected > 0);
        // Our minute computation agrees with the feed's countdown (+-1).
        int m = dep_minutes(u3, server);
        CHECK(m - u3->countdown <= 1 && u3->countdown - m <= 1);
    }
    // Every departure must be consistent with the countdown reported by WL.
    for (int i = 0; i < b.count; i++) {
        int m = dep_minutes(&b.dep[i], server);
        CHECK(abs(m - b.dep[i].countdown) <= 1);
        CHECK(b.dep[i].line[0] != 0);
        CHECK(b.dep[i].dir == 'H' || b.dep[i].dir == 'R');
    }

    // Filtering by selection.
    cfg_stop_t stop = {.kind = STOP_WL, .id = 60201468, .nsel = 2,
                       .sel = {{"U3", 'H', 4921}, {"6", 0, 0}}};
    board_clear(&b);
    CHECK_INT(wl_parse_monitor(json, &stop, 0, &b, NULL, NULL), WL_OK);
    CHECK(b.count > 0);
    for (int i = 0; i < b.count; i++) {
        bool ok = (strcmp(b.dep[i].line, "U3") == 0 && b.dep[i].dir == 'H') || strcmp(b.dep[i].line, "6") == 0;
        CHECK(ok);
    }
    CHECK(find(&b, "6", 'H') && find(&b, "6", 'R'));
    const departure_t *tram = find(&b, "6", 'H');
    if (tram) {
        CHECK_INT(tram->type, VT_TRAM);
        CHECK_STR(tram->towards, "Geiereckstra\xC3\x9F" "e");
    }

    wl_line_info_t lines[32];
    int n = wl_list_lines(json, lines, 32);
    CHECK_INT(n, 13);
    bool found = false;
    for (int i = 0; i < n; i++) {
        if (strcmp(lines[i].line, "U3") == 0 && lines[i].dir == 'H') {
            CHECK_INT(lines[i].rbl, 4921);
            found = true;
        }
    }
    CHECK(found);
    free(json);

    json = fixture_load("wl_johnstrasse_4925.json", NULL);
    board_clear(&b);
    CHECK_INT(wl_parse_monitor(json, NULL, 0, &b, NULL, NULL), WL_OK);
    CHECK_INT(b.count, WL_MAX_PER_LINE);  // one line with 13 departures, capped
    CHECK_STR(b.dep[0].towards, "Ottakring");
    free(json);

    // Error handling.
    board_clear(&b);
    CHECK_INT(wl_parse_monitor("not json", NULL, 0, &b, NULL, NULL), WL_ERR_JSON);
    CHECK_INT(wl_parse_monitor("{\"message\":{\"value\":\"Rate limit\",\"messageCode\":316}}", NULL, 0, &b, NULL,
                               &code), WL_ERR_API);
    CHECK_INT(code, 316);
    CHECK_INT(wl_parse_monitor("{\"data\":{},\"message\":{\"messageCode\":1}}", NULL, 0, &b, NULL, NULL), WL_OK);
    CHECK_INT(wl_parse_monitor("{\"data\":{\"monitors\":[{\"lines\":[{\"name\":5}]}]},\"message\":{\"messageCode\":1}}",
                               NULL, 0, &b, NULL, NULL), WL_OK);
    CHECK_INT(b.count, 0);
    // Departures without any time still get added with their countdown.
    CHECK_INT(wl_parse_monitor("{\"data\":{\"monitors\":[{\"lines\":[{\"name\":\"D\",\"towards\":\"X\",\"direction\":\"H\","
                               "\"departures\":{\"departure\":[{\"departureTime\":{\"countdown\":4}}]}}]}]},"
                               "\"message\":{\"messageCode\":1}}", NULL, 0, &b, NULL, NULL), WL_OK);
    CHECK_INT(b.count, 1);
    CHECK_INT(dep_minutes(&b.dep[0], 1792920734), 4);

    // Traffic info.
    board_clear(&b);
    CHECK_INT(wl_parse_monitor("{\"data\":{\"monitors\":[],\"trafficInfos\":[{\"title\":\"U6\",\"description\":\"Verspätung\"}]},"
                               "\"message\":{\"messageCode\":1}}", NULL, 0, &b, NULL, NULL), WL_OK);
    CHECK_STR(b.notice, "U6: Versp\xC3\xA4tung");

    char url[256];
    // A selection without platform id ("6") needs the whole station.
    wl_build_url(&stop, url, sizeof(url));
    CHECK_STR(url, WL_HOST "/ogd_realtime/monitor?activateTrafficInfo=stoerungkurz&diva=60201468");
    cfg_stop_t sel2 = {.id = 60201468, .nsel = 3, .sel = {{"U3", 'H', 4921}, {"U3", 'R', 4920}, {"U6", 'H', 4921}}};
    wl_build_url(&sel2, url, sizeof(url));
    CHECK_STR(url, WL_HOST "/ogd_realtime/monitor?activateTrafficInfo=stoerungkurz&stopId=4921&stopId=4920");
    cfg_stop_t whole = {.id = 60201468};
    wl_build_url(&whole, url, sizeof(url));
    CHECK_STR(url, WL_HOST "/ogd_realtime/monitor?activateTrafficInfo=stoerungkurz&diva=60201468");
}

static void test_wl_stations(void)
{
    char *csv = fixture_load("wl_haltestellen.csv", NULL);
    wl_station_t st[8];
    int n = wl_search_stations(csv, "Westbahnhof", st, 8);
    CHECK(n >= 2);
    CHECK_STR(st[0].name, "Westbahnhof");
    CHECK_INT(st[0].diva, 60201468);
    n = wl_search_stations(csv, "aignerstrasse", st, 8);
    CHECK_INT(n, 1);
    CHECK_STR(st[0].name, "Aignerstra\xC3\x9F" "e");
    for (int i = 0; i < n; i++) CHECK(strchr(st[i].name, '\r') == NULL);
    CHECK_INT(wl_search_stations(csv, "", st, 8), 0);
    CHECK_INT(wl_search_stations(csv, "zzzzzz", st, 8), 0);
    n = wl_search_stations(csv, "platz", st, 3);  // many hits, keep best 3
    CHECK_INT(n, 3);
    CHECK_INT(wl_search_stations("DIVA;PlatformText\n", "x", st, 8), 0);
    CHECK_INT(wl_search_stations("", "x", st, 8), 0);
    CHECK_INT(wl_search_stations("h\n1;Ab", "ab", st, 8), 0);  // malformed row
    free(csv);
}

static void test_oebb(void)
{
    char *txt = fixture_load("oebb_westbahnhof_trains.txt", NULL);
    board_t b;
    board_clear(&b);
    char station[48];
    CHECK_INT(oebb_parse_board(txt, NULL, 1, &b, station, sizeof(station)), 10);
    CHECK_STR(station, "Wien Westbahnhof (U)");
    CHECK_STR(b.dep[0].line, "VAB 1");
    CHECK_STR(b.dep[0].towards, "Flughafen Wien");
    CHECK_INT(b.dep[0].stop, 1);
    CHECK_STR(b.dep[2].line, "REX 51");
    CHECK_STR(b.dep[2].towards, "St.P\xC3\xB6lten Hbf");
    CHECK_STR(b.dep[2].platform, "5");
    CHECK_INT(b.dep[2].planned, tu_vienna_to_utc(2026, 10, 25, 10, 53));
    CHECK_INT(b.dep[2].type, VT_TRAIN);
    CHECK_INT(b.dep[1].type, VT_SBAHN);
    free(txt);

    txt = fixture_load("oebb_westbahnhof_all.txt", NULL);
    board_clear(&b);
    int n = oebb_parse_board(txt, NULL, 0, &b, NULL, 0);
    CHECK_INT(n, 7);  // 8 journeys, one trip listed twice
    const departure_t *tram6 = find(&b, "6", 0);
    CHECK(tram6 != NULL);
    if (tram6) {
        CHECK(tram6->flags & DEP_F_REALTIME);
        CHECK_INT(tram6->expected, tu_vienna_to_utc(2026, 10, 25, 10, 32));
        CHECK(dep_is_delayed(tram6));
        CHECK_INT(tram6->type, VT_TRAM);
    }
    CHECK(find(&b, "U6", 0) && find(&b, "U6", 0)->type == VT_METRO);

    // Line filter accepts both the short and the product name.
    cfg_stop_t stop = {.kind = STOP_OEBB, .id = 1291501, .nsel = 2, .sel = {{"52"}, {"U6"}}};
    board_clear(&b);
    oebb_parse_board(txt, &stop, 0, &b, NULL, 0);
    CHECK_INT(b.count, 4);
    free(txt);

    // Cancelled journey and malformed input.
    board_clear(&b);
    CHECK_INT(oebb_parse_board("x={\"journey\":[{\"id\":\"1\",\"ti\":\"10:00\",\"da\":\"25.10.2026\",\"pr\":\"S 7\","
                               "\"st\":\"Flughafen\",\"rt\":{\"status\":\"Ausfall\",\"dlt\":\"\"}}]};",
                               NULL, 0, &b, NULL, 0), 1);
    CHECK(b.dep[0].flags & DEP_F_CANCELLED);
    CHECK_INT(b.dep[0].expected, 0);
    CHECK_INT(oebb_parse_board("", NULL, 0, &b, NULL, 0), -1);
    CHECK_INT(oebb_parse_board("journeysObj = {", NULL, 0, &b, NULL, 0), -1);
    CHECK_INT(oebb_parse_board("x={\"journey\":[{\"pr\":\"S 7\"}]}", NULL, 0, &b, NULL, 0), 0);
    CHECK_INT(oebb_parse_board("<html>Service unavailable</html>", NULL, 0, &b, NULL, 0), -1);

    txt = fixture_load("oebb_getstop_westbahnhof.txt", NULL);
    oebb_station_t st[4];
    CHECK_INT(oebb_parse_search(txt, st, 4), 1);
    CHECK_INT(st[0].eva, 1291501);
    CHECK_STR(st[0].name, "Wien Westbahnhof (U)");
    free(txt);

    // The live station search answers in Latin-1 ("H\xFCtteldorf").
    CHECK_INT(oebb_parse_search("SLs.sls={\"suggestions\":[{\"value\":\"Wien H\xFCtteldorf\",\"extId\":\"001291601\","
                                "\"type\":\"1\"},{\"value\":\"Hauptstra\xDF" "e 1\",\"extId\":\"9\",\"type\":\"2\"}]};",
                                st, 4), 1);
    CHECK_STR(st[0].name, "Wien H\xC3\xBCtteldorf");
    CHECK_INT(st[0].eva, 1291601);
    CHECK(tx_is_utf8("Stra\xC3\x9F" "e"));
    CHECK(!tx_is_utf8("Stra\xDF" "e"));
    CHECK(!tx_is_utf8("\xC3"));
    CHECK(!tx_is_utf8("\xC0\x80"));  // overlong
    char *u = tx_latin1_to_utf8("\xC4\xD6\xDC\xE4\xF6\xFC\xDF");
    CHECK_STR(u, "\xC3\x84\xC3\x96\xC3\x9C\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F");
    free(u);

    char line[16];
    oebb_short_line("Tram 52", line, sizeof(line));
    CHECK_STR(line, "52");
    oebb_short_line("BusVAB 1", line, sizeof(line));
    CHECK_STR(line, "VAB 1");
    oebb_short_line("Bus 13A", line, sizeof(line));
    CHECK_STR(line, "13A");
    oebb_short_line("S 50", line, sizeof(line));
    CHECK_STR(line, "S 50");
    CHECK_INT(oebb_vehicle_type("S 50"), VT_SBAHN);
    CHECK_INT(oebb_vehicle_type("RJX 162"), VT_TRAIN);
    CHECK_INT(oebb_vehicle_type("U6"), VT_METRO);

    char url[512];
    cfg_stop_t s2 = {.kind = STOP_OEBB, .id = 1291501, .trains_only = true};
    oebb_build_board_url(&s2, tu_vienna_to_utc(2026, 10, 25, 10, 30), 12, url, sizeof(url));
    CHECK(strstr(url, "evaId=1291501&maxJourneys=12&productsFilter=1111110000000000") != NULL);
    CHECK(strstr(url, "&time=10:30&dateBegin=25.10.2026&dateEnd=25.10.2026") != NULL);
    oebb_build_search_url("Wien Hütteldorf", url, sizeof(url));
    CHECK(strstr(url, "&S=Wien%20H%C3%BCtteldorf") != NULL);
}

static void test_config(void)
{
    app_config_t c;
    cfg_defaults(&c);
    CHECK_STR(c.hostname, "bimfo");
    CHECK_INT(c.refresh_s, 30);
    CHECK(!c.auto_update);  // automatic updates are opt-in
    char err[64];

    const char *json =
        "{\"wifi\":{\"ssid\":\"Zuhause\",\"pass\":\"geheim123\"},\"hostname\":\"Küchen Monitor!\","
        "\"theme\":\"oebb\",\"group\":false,\"brightness\":500,\"refresh\":5,\"flip\":true,\"auto_update\":true,"
        "\"night\":{\"brightness\":5,\"start\":\"23:15\",\"end\":\"6:05\"},"
        "\"stops\":[{\"kind\":\"wl\",\"name\":\"Westbahnhof\",\"id\":60201468,\"walk\":4,"
        "\"sel\":[{\"line\":\"U3\",\"dir\":\"H\",\"rbl\":4921},{\"line\":\"\"},{\"line\":\"6\",\"dir\":\"X\"}]},"
        "{\"kind\":\"oebb\",\"name\":\"Wien Westbf\",\"id\":1291501,\"trains_only\":true},"
        "{\"kind\":\"wl\",\"name\":\"no id\"},{\"id\":1},{\"id\":2},{\"id\":3},{\"id\":4}]}";
    CHECK(cfg_from_json(&c, json, err, sizeof(err)));
    CHECK_STR(c.wifi_ssid, "Zuhause");
    CHECK_STR(c.wifi_pass, "geheim123");
    CHECK_STR(c.hostname, "kchenmonitor");
    CHECK_INT(c.theme, THEME_OEBB);
    CHECK(!c.group_lines);
    CHECK_INT(c.brightness, 100);
    CHECK_INT(c.refresh_s, 15);
    CHECK(c.flip);
    CHECK(c.auto_update);
    CHECK_INT(c.night_brightness, 5);
    CHECK_INT(c.night_start, 23 * 60 + 15);
    CHECK_INT(c.night_end, 6 * 60 + 5);
    CHECK_INT(c.nstops, CFG_MAX_STOPS);  // "no id" skipped, rest capped
    CHECK_INT(c.stops[0].nsel, 2);
    CHECK_INT(c.stops[0].sel[1].dir, 0);  // "X" is not a direction
    CHECK_INT(c.stops[0].walk_min, 4);
    CHECK_INT(c.stops[1].kind, STOP_OEBB);
    CHECK(c.stops[1].trains_only);

    // Public JSON hides the password; a save without "pass" keeps it.
    char *pub = cfg_to_json(&c, false);
    CHECK(strstr(pub, "geheim123") == NULL);
    CHECK(strstr(pub, "\"has_pass\":true") != NULL);
    app_config_t c2;
    cfg_defaults(&c2);
    strcpy(c2.wifi_pass, "old");
    CHECK(cfg_from_json(&c2, pub, err, sizeof(err)));
    CHECK_STR(c2.wifi_pass, "old");
    CHECK_INT(c2.nstops, c.nstops);
    CHECK_INT(c2.stops[0].sel[0].rbl, 4921);
    CHECK_INT(c2.night_end, c.night_end);
    CHECK(c2.auto_update);
    free(pub);

    // Full round trip including secrets.
    char *full = cfg_to_json(&c, true);
    app_config_t c3;
    cfg_defaults(&c3);
    CHECK(cfg_from_json(&c3, full, err, sizeof(err)));
    char *again = cfg_to_json(&c3, true);
    CHECK_STR(again, full);
    free(full);
    free(again);

    // Clearing the SSID clears the password; malformed JSON changes nothing.
    CHECK(cfg_from_json(&c3, "{\"wifi\":{\"ssid\":\"\"}}", err, sizeof(err)));
    CHECK_STR(c3.wifi_pass, "");
    app_config_t before = c3;
    CHECK(!cfg_from_json(&c3, "{\"theme\":", err, sizeof(err)));
    CHECK(memcmp(&before, &c3, sizeof(c3)) == 0);
    CHECK(!cfg_from_json(&c3, "[1,2]", err, sizeof(err)));
    CHECK(cfg_from_json(&c3, "{\"hostname\":\"---\"}", err, sizeof(err)));
    CHECK_STR(c3.hostname, "bimfo");

    // Night window 23:15-06:05 (Vienna local), brightness 100 / night 5.
    CHECK_INT(cfg_brightness(&c, tu_vienna_to_utc(2026, 10, 25, 23, 14)), 100);
    CHECK_INT(cfg_brightness(&c, tu_vienna_to_utc(2026, 10, 25, 23, 15)), 5);
    CHECK_INT(cfg_brightness(&c, tu_vienna_to_utc(2026, 10, 26, 3, 0)), 5);
    CHECK_INT(cfg_brightness(&c, tu_vienna_to_utc(2026, 10, 26, 6, 5)), 100);
    CHECK_INT(cfg_brightness(&c, tu_vienna_to_utc(2026, 7, 1, 23, 30)), 5);  // summer time
    CHECK_INT(cfg_brightness(&c, 0), 100);  // unknown time: day brightness
    app_config_t day = c;
    day.night_start = 9 * 60;
    day.night_end = 17 * 60;  // window within one day
    CHECK_INT(cfg_brightness(&day, tu_vienna_to_utc(2026, 10, 26, 12, 0)), 5);
    CHECK_INT(cfg_brightness(&day, tu_vienna_to_utc(2026, 10, 26, 18, 0)), 100);
    day.night_end = day.night_start;  // disabled
    CHECK_INT(cfg_brightness(&day, tu_vienna_to_utc(2026, 10, 26, 12, 0)), 100);

    char title[48];
    cfg_board_title(&c, title, sizeof(title));
    CHECK_STR(title, "Westbahnhof \xC2\xB7 Wien Westbf \xC2\xB7  \xC2\xB7 ");
    app_config_t one;
    cfg_defaults(&one);
    cfg_board_title(&one, title, sizeof(title));
    CHECK_STR(title, "");
    one.nstops = 1;
    strcpy(one.stops[0].name, "Gumpendorfer Stra\xC3\x9F" "e");
    cfg_board_title(&one, title, sizeof(title));
    CHECK_STR(title, "Gumpendorfer Stra\xC3\x9F" "e");
    one.nstops = 2;
    strcpy(one.stops[1].name, "Ein sehr langer Haltestellenname der nicht passt");
    // "Gumpendorfer Straße" is 20 bytes, then " " and the 2-byte middle dot at
    // bytes 21..22: a 22-byte limit must cut before the dot, never inside it.
    cfg_board_title(&one, title, 23);
    CHECK_INT(strlen(title), 21);

    cfg_stop_t s = {.nsel = 1, .sel = {{"U3", 'H'}}};
    CHECK(cfg_stop_accepts(&s, "U3", 'H'));
    CHECK(!cfg_stop_accepts(&s, "U3", 'R'));
    CHECK(!cfg_stop_accepts(&s, "U6", 'H'));
    s.nsel = 0;
    CHECK(cfg_stop_accepts(&s, "anything", 0));
}

static void test_board(void)
{
    board_t b;
    board_clear(&b);
    const int64_t now = 1792920734;
    const struct {
        const char *line, *towards;
        int min;
        uint8_t stop;
        uint8_t flags;
    } in[] = {
        {"U3", "Ottakring", 7, 0, 0}, {"U3", "Simmering", 2, 0, 0}, {"U3", "Ottakring", 1, 0, 0},
        {"52", "Baumgarten", 0, 1, 0}, {"U3", "Ottakring", 12, 0, 0}, {"S 50", "Purkersdorf", 3, 1, DEP_F_CANCELLED},
        {"6", "Burggasse", -3, 0, 0},
    };
    for (size_t i = 0; i < sizeof(in) / sizeof(in[0]); i++) {
        departure_t *d = board_add(&b);
        strcpy(d->line, in[i].line);
        strcpy(d->towards, in[i].towards);
        d->planned = now + in[i].min * 60 + 30;
        d->countdown = (int16_t)in[i].min;
        d->stop = in[i].stop;
        d->flags = in[i].flags;
    }
    board_sort(&b);
    CHECK_STR(b.dep[0].line, "6");
    CHECK_STR(b.dep[1].line, "52");
    CHECK_INT(dep_minutes(&b.dep[1], now), 0);
    CHECK_INT(dep_minutes(&b.dep[0], now), 0);
    CHECK_INT(dep_minutes(&b.dep[2], now), 1);

    board_row_t rows[8];
    int n = board_rows(&b, now, true, NULL, rows, 8);
    CHECK_INT(n, 3);  // 52, U3 Ottakring (1, next 7), U3 Simmering; departed + cancelled hidden
    CHECK_STR(rows[0].dep->line, "52");
    CHECK_STR(rows[1].dep->towards, "Ottakring");
    CHECK_INT(rows[1].next_minutes, 7);
    CHECK_INT(rows[2].next_minutes, -1);

    // Rows full: the next time must still be found further down the list.
    n = board_rows(&b, now, true, NULL, rows, 2);
    CHECK_INT(n, 2);
    CHECK_INT(rows[1].next_minutes, 7);

    n = board_rows(&b, now, false, NULL, rows, 8);
    CHECK_INT(n, 6);  // cancelled stays visible in list mode
    n = board_rows(&b, now, false, NULL, rows, 2);
    CHECK_INT(n, 2);

    uint8_t walk[BOARD_MAX_STOPS] = {5, 0};
    n = board_rows(&b, now, false, walk, rows, 8);
    CHECK_INT(n, 4);  // stop 0 needs >= 5 min: only U3 7/12; stop 1: 52, S 50
    // Without a clock nothing is dropped as "departed".
    n = board_rows(&b, 0, false, NULL, rows, 8);
    CHECK_INT(n, 7);

    board_t big, merged;
    board_clear(&big);
    board_clear(&merged);
    for (int i = 0; i < 50; i++) {
        departure_t *d = board_add(&big);
        snprintf(d->line, sizeof(d->line), "L%d", i);
        d->planned = now + (50 - i) * 60;  // reverse order: sorting matters
    }
    strcpy(big.notice, "Störung");
    board_append_soonest(&merged, &big, 10);
    CHECK_INT(merged.count, 10);
    CHECK_STR(merged.dep[0].line, "L49");
    CHECK_STR(merged.notice, "Störung");
    board_append_soonest(&merged, &b, 100);  // fewer than max: all
    CHECK_INT(merged.count, 10 + b.count);
    CHECK_STR(merged.notice, "Störung");

    board_t kept;
    board_clear(&kept);
    CHECK_INT(board_copy_stop(&kept, &b, 1), 2);  // 52 and S 50
    CHECK_INT(kept.count, 2);
    CHECK_INT(board_copy_stop(&kept, &b, 3), 0);

    board_clear(&b);
    for (int i = 0; i < BOARD_MAX_DEPS; i++) CHECK(board_add(&b) != NULL);
    CHECK(board_add(&b) == NULL);
    board_t c;
    board_clear(&c);
    board_add(&c);
    board_append(&b, &c);
    CHECK_INT(b.count, BOARD_MAX_DEPS);

    departure_t d = {.planned = now + 120, .expected = now + 120 + 59};
    CHECK(!dep_is_delayed(&d));
    d.expected = now + 180;
    CHECK(dep_is_delayed(&d));
}

static void test_dns(void)
{
    // Query for "abc.de" type A, class IN.
    uint8_t q[512] = {0x12, 0x34, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0, 3, 'a', 'b', 'c', 2, 'd', 'e', 0, 0, 1, 0, 1};
    uint32_t ip = 0x0104A8C0;  // 192.168.4.1 in network byte order on little endian
    int n = dns_build_reply(q, 24, sizeof(q), ip);
    CHECK_INT(n, 24 + 16);
    CHECK_INT(q[0], 0x12);   // id kept
    CHECK_INT(q[2], 0x81);   // response
    CHECK_INT(q[7], 1);      // one answer
    CHECK_INT(q[24], 0xC0);  // name pointer
    CHECK_INT(q[24 + 11], 4);
    CHECK(memcmp(q + 24 + 12, &ip, 4) == 0);

    uint8_t aaaa[64] = {0, 1, 0x01, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 'x', 0, 0, 28, 0, 1};
    CHECK_INT(dns_build_reply(aaaa, 19, sizeof(aaaa), ip), 19);
    CHECK_INT(aaaa[7], 0);   // no answer for AAAA

    uint8_t bad[64] = {0};
    CHECK_INT(dns_build_reply(bad, 5, sizeof(bad), ip), 0);
    bad[5] = 1;
    bad[12] = 60;            // label running past the packet
    CHECK_INT(dns_build_reply(bad, 20, sizeof(bad), ip), 0);
    uint8_t resp[24] = {0, 1, 0x81, 0x80, 0, 1};  // already a response
    CHECK_INT(dns_build_reply(resp, 24, sizeof(resp), ip), 0);
    // No room for the answer record.
    uint8_t tight[24] = {0x12, 0x34, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0, 3, 'a', 'b', 'c', 2, 'd', 'e', 0, 0, 1, 0, 1};
    CHECK_INT(dns_build_reply(tight, 24, 24, ip), 0);
}

static void test_api_json(void)
{
    char *json = fixture_load("wl_westbahnhof_diva.json", NULL);
    wl_line_info_t lines[32];
    int n = wl_list_lines(json, lines, 32);
    cJSON *arr = api_wl_lines_json(lines, n);
    CHECK_INT(cJSON_GetArraySize(arr), 13);
    // Sorted: U-Bahn first (type order), then trams by number: 5, 6, 9, 18, 52, 60.
    CHECK_STR(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 0), "line")->valuestring, "U3");
    CHECK_STR(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 0), "type")->valuestring, "metro");
    CHECK_STR(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 4), "line")->valuestring, "5");
    CHECK_STR(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 12), "line")->valuestring, "60");
    cJSON_Delete(arr);

    board_t b;
    board_clear(&b);
    int64_t server;
    wl_parse_monitor(json, NULL, 0, &b, &server, NULL);
    free(json);
    cJSON *o = api_board_json(&b, server);
    cJSON *deps = cJSON_GetObjectItem(o, "departures");
    CHECK_INT(cJSON_GetArraySize(deps), b.count);
    cJSON *d0 = cJSON_GetArrayItem(deps, 0);
    CHECK(cJSON_IsNumber(cJSON_GetObjectItem(d0, "minutes")));
    CHECK(cJSON_IsBool(cJSON_GetObjectItem(d0, "barrier_free")));
    CHECK_INT(cJSON_GetObjectItem(o, "now")->valuedouble, server);
    cJSON_Delete(o);

    wl_station_t st[1] = {{60201468, "Westbahnhof"}};
    o = api_wl_stations_json(st, 1);
    char *s = cJSON_PrintUnformatted(o);
    CHECK_STR(s, "[{\"id\":60201468,\"name\":\"Westbahnhof\"}]");
    free(s);
    cJSON_Delete(o);
}

static void test_update(void)
{
    int v[3];
    CHECK(upd_parse_version("1.2.3", true, v) && v[0] == 1 && v[1] == 2 && v[2] == 3);
    CHECK(upd_parse_version("v10.0.12", true, v) && v[0] == 10 && v[2] == 12);
    CHECK(!upd_parse_version("1.2.3-rc1", true, v));
    CHECK(upd_parse_version("1.2.3-dirty", false, v) && v[2] == 3);
    CHECK(!upd_parse_version("1.2", false, v));
    CHECK(!upd_parse_version("1..3", false, v));
    CHECK(!upd_parse_version("", false, v));
    CHECK(!upd_parse_version(NULL, false, v));
    CHECK(!upd_parse_version("1.2.999999", false, v));

    CHECK(upd_is_newer("1.0.1", "1.0.0"));
    CHECK(upd_is_newer("1.1.0", "1.0.9"));
    CHECK(upd_is_newer("2.0.0", "1.99.99"));
    CHECK(upd_is_newer("1.10.0", "1.9.0"));         // numbers, not text
    CHECK(!upd_is_newer("1.0.0", "1.0.0"));         // same version: nothing to do
    CHECK(!upd_is_newer("0.9.9", "1.0.0"));         // never downgrade
    CHECK(!upd_is_newer("1.1.0-rc1", "1.0.0"));     // pre-releases are not installed
    CHECK(upd_is_newer("1.0.1", "1.0.0-dirty"));    // a development build updates by its numbers
    CHECK(!upd_is_newer("1.0.1", "unknown"));
    CHECK(!upd_is_newer("bimfo", "1.0.0"));

    // Next check: 03:00 Vienna time strictly after now, plus the jitter.
    int64_t evening = tu_vienna_to_utc(2026, 10, 27, 21, 0);
    CHECK_INT(upd_next_check(evening, 0), tu_vienna_to_utc(2026, 10, 28, 3, 0));
    CHECK_INT(upd_next_check(evening, 1234), tu_vienna_to_utc(2026, 10, 28, 3, 0) + 1234);
    CHECK_INT(upd_next_check(evening, UPD_JITTER_S + 5), tu_vienna_to_utc(2026, 10, 28, 3, 0) + 5);
    int64_t night = tu_vienna_to_utc(2026, 10, 28, 1, 30);
    CHECK_INT(upd_next_check(night, 0), tu_vienna_to_utc(2026, 10, 28, 3, 0));
    // At or after 03:00 (a check that just ran) the next one is tomorrow.
    int64_t three = tu_vienna_to_utc(2026, 10, 28, 3, 0);
    CHECK_INT(upd_next_check(three, 0), tu_vienna_to_utc(2026, 10, 29, 3, 0));
    CHECK_INT(upd_next_check(three + 1800, 0), tu_vienna_to_utc(2026, 10, 29, 3, 0));
    // Month and year ends, and both clock changes.
    CHECK_INT(upd_next_check(tu_vienna_to_utc(2026, 12, 31, 12, 0), 0), tu_vienna_to_utc(2027, 1, 1, 3, 0));
    CHECK_INT(upd_next_check(tu_vienna_to_utc(2026, 3, 28, 22, 0), 0), tu_vienna_to_utc(2026, 3, 29, 3, 0));
    CHECK_INT(upd_next_check(tu_vienna_to_utc(2026, 10, 24, 22, 0), 0), tu_vienna_to_utc(2026, 10, 25, 3, 0));
    CHECK_INT(tu_vienna_to_utc(2026, 3, 29, 3, 0) - tu_vienna_to_utc(2026, 3, 28, 3, 0), 23 * 3600);
    tu_fields_t f;
    tu_vienna_fields(upd_next_check(tu_vienna_to_utc(2026, 10, 24, 22, 0), 0), &f);
    CHECK_INT(f.hour, 3);
}

int main(void)
{
    test_time();
    test_text();
    test_wl();
    test_wl_stations();
    test_oebb();
    test_config();
    test_update();
    test_board();
    test_dns();
    test_api_json();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
