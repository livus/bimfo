#include "oebb_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "textutil.h"
#include "timeutil.h"

// HAFAS product classes, one flag per position: RJ/ICE, IC/EC, night trains,
// regional express, regional, S-Bahn, bus, ship, U-Bahn, tram, ... ("1" = include).
#define OEBB_PRODUCTS_TRAINS "1111110000000000"
#define OEBB_PRODUCTS_ALL    "1111111111111111"

static void url_encode(const char *in, char *out, size_t n)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t w = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < n; p++) {
        if (isalnum(*p) || *p == '-' || *p == '.' || *p == '_') {
            out[w++] = (char)*p;
        } else {
            out[w++] = '%';
            out[w++] = hex[*p >> 4];
            out[w++] = hex[*p & 15];
        }
    }
    out[w] = 0;
}

void oebb_build_board_url(const cfg_stop_t *stop, int64_t now, int max_journeys, char *buf, size_t n)
{
    tu_fields_t f;
    tu_vienna_fields(now, &f);
    snprintf(buf, n,
             OEBB_HOST "/bin/stboard.exe/dn?L=vs_scotty.vs_liveticker&boardType=dep"
             "&outputMode=tickerDataOnly&start=yes&selectDate=period&additionalTime=0"
             "&evaId=%ld&maxJourneys=%d&productsFilter=%s"
             "&time=%02d:%02d&dateBegin=%02d.%02d.%04d&dateEnd=%02d.%02d.%04d",
             (long)stop->id, max_journeys, stop->trains_only ? OEBB_PRODUCTS_TRAINS : OEBB_PRODUCTS_ALL,
             f.hour, f.min, f.day, f.month, f.year, f.day, f.month, f.year);
}

void oebb_build_search_url(const char *query, char *buf, size_t n)
{
    char q[96];
    url_encode(query, q, sizeof(q));
    snprintf(buf, n, OEBB_HOST "/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1&REQ0JourneyStopsB=12&js=true&S=%s", q);
}

uint8_t oebb_vehicle_type(const char *pr)
{
    if (strncmp(pr, "Tram", 4) == 0) return VT_TRAM;
    if (strncmp(pr, "Bus", 3) == 0) return VT_BUS;
    if (strncmp(pr, "NightJet", 8) == 0) return VT_TRAIN;
    if (pr[0] == 'U' && isdigit((unsigned char)pr[1])) return VT_METRO;
    if (pr[0] == 'S' && (pr[1] == ' ' || isdigit((unsigned char)pr[1]))) return VT_SBAHN;
    return VT_TRAIN;
}

void oebb_short_line(const char *pr, char *out, size_t n)
{
    static const char *const prefixes[] = {"Tram ", "Bus "};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t l = strlen(prefixes[i]);
        if (strncmp(pr, prefixes[i], l) == 0) {
            pr += l;
            break;
        }
    }
    // "BusVAB 1" (Vienna Airport Lines) -> "VAB 1"
    if (strncmp(pr, "Bus", 3) == 0 && isupper((unsigned char)pr[3])) {
        pr += 3;
    }
    tx_copy(out, pr, n);
    tx_trim(out);
}

// The payload is "name = { ... };" (board) or "SLs.sls={...};SLs.showSuggestion();".
// The station search answers in Latin-1, so anything that is not valid UTF-8
// is converted first.
static cJSON *parse_embedded(const char *payload)
{
    if (!payload) return NULL;
    char *converted = NULL;
    if (!tx_is_utf8(payload)) {
        converted = tx_latin1_to_utf8(payload);
        if (!converted) return NULL;
        payload = converted;
    }
    const char *start = strchr(payload, '{');
    const char *end = strrchr(payload, '}');
    cJSON *root = NULL;
    if (start && end && end > start) {
        root = cJSON_ParseWithLength(start, (size_t)(end - start + 1));
    }
    free(converted);
    return root;
}

static const char *jstr(const cJSON *obj, const char *key)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(it) ? it->valuestring : NULL;
}

int oebb_parse_board(const char *payload, const cfg_stop_t *stop, uint8_t stop_idx,
                     board_t *out, char *station, size_t station_len)
{
    cJSON *root = parse_embedded(payload);
    if (!root) return -1;
    if (station) {
        tx_copy(station, jstr(root, "stationName"), station_len);
        tx_html_decode(station);
    }
    int added = 0;
    // Stations with several stop points (e.g. tram stops around a station)
    // list the same trip once per stop point; keep only the first.
    char seen[BOARD_MAX_DEPS][16];
    int nseen = 0;
    const cJSON *j;
    cJSON_ArrayForEach(j, cJSON_GetObjectItemCaseSensitive(root, "journey")) {
        const char *pr = jstr(j, "pr");
        const char *ti = jstr(j, "ti");
        const char *da = jstr(j, "da");
        const char *id = jstr(j, "id");
        if (!pr || !ti || !da) continue;
        if (id) {
            bool dup = false;
            for (int k = 0; k < nseen && !dup; k++) dup = strcmp(seen[k], id) == 0;
            if (dup) continue;
            if (nseen < BOARD_MAX_DEPS) tx_copy(seen[nseen++], id, sizeof(seen[0]));
        }

        char line[DEP_LINE_LEN];
        oebb_short_line(pr, line, sizeof(line));
        if (stop && !cfg_stop_accepts(stop, line, 0) && !cfg_stop_accepts(stop, pr, 0)) {
            continue;
        }
        departure_t *d = board_add(out);
        if (!d) break;
        tx_copy(d->line, line, sizeof(d->line));
        tx_copy(d->towards, jstr(j, "st"), sizeof(d->towards));
        tx_html_decode(d->towards);
        tx_copy(d->platform, jstr(j, "tr"), sizeof(d->platform));
        d->type = oebb_vehicle_type(pr);
        d->stop = stop_idx;
        d->planned = tu_parse_vienna_date_time(da, ti);

        // "rt" is false without realtime data, else {status, dlm, dlt, dld}.
        const cJSON *rt = cJSON_GetObjectItemCaseSensitive(j, "rt");
        if (cJSON_IsObject(rt)) {
            d->flags |= DEP_F_REALTIME;
            const char *status = jstr(rt, "status");
            if (status && status[0]) {
                d->flags |= DEP_F_CANCELLED;
            }
            const char *dlt = jstr(rt, "dlt");
            const char *dld = jstr(rt, "dld");
            if (dlt && dlt[0]) {
                d->expected = tu_parse_vienna_date_time(dld && dld[0] ? dld : da, dlt);
            }
        }
        added++;
    }
    cJSON_Delete(root);
    return added;
}

int oebb_parse_search(const char *payload, oebb_station_t *out, int max)
{
    cJSON *root = parse_embedded(payload);
    if (!root) return -1;
    int n = 0;
    const cJSON *s;
    cJSON_ArrayForEach(s, cJSON_GetObjectItemCaseSensitive(root, "suggestions")) {
        const char *type = jstr(s, "type");
        const char *ext = jstr(s, "extId");
        const char *name = jstr(s, "value");
        // type 1 = station; 2 = address, 4 = point of interest
        if (n >= max || !ext || !name || (type && strcmp(type, "1") != 0)) continue;
        out[n].eva = (int32_t)strtol(ext, NULL, 10);
        tx_copy(out[n].name, name, sizeof(out[n].name));
        tx_html_decode(out[n].name);
        n++;
    }
    cJSON_Delete(root);
    return n;
}
