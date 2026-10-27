#include "wl_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "textutil.h"
#include "timeutil.h"

void wl_build_url(const cfg_stop_t *stop, char *buf, size_t n)
{
    int len = snprintf(buf, n, WL_HOST "/ogd_realtime/monitor?activateTrafficInfo=stoerungkurz");
    // Platform ids keep the response small, but only if every selected line
    // has one; otherwise ask for the whole station and filter locally.
    bool all_rbl = stop->nsel > 0;
    for (int i = 0; i < stop->nsel; i++) {
        all_rbl = all_rbl && stop->sel[i].rbl > 0;
    }
    if (!all_rbl) {
        snprintf(buf + len, len < (int)n ? n - len : 0, "&diva=%ld", (long)stop->id);
        return;
    }
    int32_t seen[CFG_MAX_SEL];
    int nseen = 0;
    for (int i = 0; i < stop->nsel; i++) {
        int32_t rbl = stop->sel[i].rbl;
        bool dup = false;
        for (int k = 0; k < nseen && !dup; k++) dup = seen[k] == rbl;
        if (dup) continue;
        seen[nseen++] = rbl;
        len += snprintf(buf + len, len < (int)n ? n - len : 0, "&stopId=%ld", (long)rbl);
    }
}

uint8_t wl_vehicle_type(const char *type)
{
    if (!type) return VT_UNKNOWN;
    if (strcmp(type, "ptMetro") == 0) return VT_METRO;
    if (strncmp(type, "ptTram", 6) == 0) return VT_TRAM;
    if (strcmp(type, "ptBusNight") == 0) return VT_NIGHTBUS;
    if (strncmp(type, "ptBus", 5) == 0 || strcmp(type, "ptRufBus") == 0) return VT_BUS;
    if (strcmp(type, "ptTrainS") == 0) return VT_SBAHN;
    if (strncmp(type, "ptTrain", 7) == 0) return VT_TRAIN;
    return VT_UNKNOWN;
}

static const char *jstr(const cJSON *obj, const char *key)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(it) ? it->valuestring : NULL;
}

static bool jbool(const cJSON *obj, const char *key, bool def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsBool(it) ? cJSON_IsTrue(it) : def;
}

static const cJSON *jpath(const cJSON *obj, const char *a, const char *b)
{
    return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(obj, a), b);
}

// Collects the first traffic info that mentions one of our lines.
static void parse_traffic_info(const cJSON *data, board_t *out)
{
    if (out->notice[0]) return;
    const cJSON *info;
    cJSON_ArrayForEach(info, cJSON_GetObjectItemCaseSensitive(data, "trafficInfos")) {
        const char *title = jstr(info, "title");
        const char *desc = jstr(info, "description");
        if (!title && !desc) continue;
        snprintf(out->notice, sizeof(out->notice), "%s%s%s", title ? title : "",
                 title && desc ? ": " : "", desc ? desc : "");
        return;
    }
}

wl_status_t wl_parse_monitor(const char *json, const cfg_stop_t *stop, uint8_t stop_idx,
                             board_t *out, int64_t *server_time, int *api_code)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return WL_ERR_JSON;
    }
    wl_status_t rc = WL_OK;
    const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "message");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(msg, "messageCode");
    if (api_code) {
        *api_code = cJSON_IsNumber(code) ? code->valueint : -1;
    }
    if (server_time) {
        int64_t t = 0;
        tu_parse_iso8601(jstr(msg, "serverTime"), &t);
        *server_time = t;
    }
    if (!cJSON_IsNumber(code) || code->valueint != 1) {
        rc = WL_ERR_API;
        goto done;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *monitor;
    cJSON_ArrayForEach(monitor, cJSON_GetObjectItemCaseSensitive(data, "monitors")) {
        const cJSON *line;
        cJSON_ArrayForEach(line, cJSON_GetObjectItemCaseSensitive(monitor, "lines")) {
            const char *name = jstr(line, "name");
            const char *dir = jstr(line, "direction");
            if (!name || (stop && !cfg_stop_accepts(stop, name, dir ? dir[0] : 0))) {
                continue;
            }
            const char *towards = jstr(line, "towards");
            const char *type = jstr(line, "type");
            const char *platform = jstr(line, "platform");
            bool barrier_free = jbool(line, "barrierFree", false);
            bool realtime = jbool(line, "realtimeSupported", false);

            const cJSON *dep;
            int taken = 0;
            cJSON_ArrayForEach(dep, jpath(line, "departures", "departure")) {
                // A whole station lists ~10 departures for each of up to a dozen
                // lines; keep the next few per line so no line gets crowded out.
                if (taken++ >= WL_MAX_PER_LINE) break;
                const cJSON *dt = cJSON_GetObjectItemCaseSensitive(dep, "departureTime");
                const cJSON *veh = cJSON_GetObjectItemCaseSensitive(dep, "vehicle");
                departure_t *d = board_add(out);
                if (!d) goto done;

                tx_copy(d->line, name, sizeof(d->line));
                // A vehicle entry overrides line defaults, e.g. short workings.
                const char *vt = veh ? jstr(veh, "towards") : NULL;
                tx_copy(d->towards, vt && vt[0] ? vt : towards, sizeof(d->towards));
                tx_trim(d->towards);
                const char *vp = veh ? jstr(veh, "platform") : NULL;
                tx_copy(d->platform, vp ? vp : platform, sizeof(d->platform));
                d->dir = dir ? dir[0] : 0;
                d->type = wl_vehicle_type(veh && jstr(veh, "type") ? jstr(veh, "type") : type);
                d->stop = stop_idx;

                tu_parse_iso8601(jstr(dt, "timePlanned"), &d->planned);
                if (tu_parse_iso8601(jstr(dt, "timeReal"), &d->expected)) {
                    d->flags |= DEP_F_REALTIME;
                }
                const cJSON *cd = cJSON_GetObjectItemCaseSensitive(dt, "countdown");
                d->countdown = (int16_t)(cJSON_IsNumber(cd) ? cd->valueint : -1);

                bool bf = veh ? jbool(veh, "barrierFree", barrier_free) : barrier_free;
                if (bf) d->flags |= DEP_F_BARRIER_FREE;
                if (!realtime) d->flags &= (uint8_t)~DEP_F_REALTIME;
            }
        }
    }
    parse_traffic_info(data, out);

done:
    cJSON_Delete(root);
    return rc;
}

int wl_list_lines(const char *json, wl_line_info_t *out, int max)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;
    int n = 0;
    const cJSON *monitor;
    cJSON_ArrayForEach(monitor, jpath(root, "data", "monitors")) {
        const cJSON *attrs = jpath(monitor, "locationStop", "properties");
        const cJSON *rbl = jpath(attrs, "attributes", "rbl");
        const cJSON *line;
        cJSON_ArrayForEach(line, cJSON_GetObjectItemCaseSensitive(monitor, "lines")) {
            const char *name = jstr(line, "name");
            const char *dir = jstr(line, "direction");
            if (!name || n >= max) continue;
            wl_line_info_t *e = &out[n];
            tx_copy(e->line, name, sizeof(e->line));
            tx_copy(e->towards, jstr(line, "towards"), sizeof(e->towards));
            tx_trim(e->towards);
            e->dir = dir ? dir[0] : 0;
            e->rbl = cJSON_IsNumber(rbl) ? rbl->valueint : 0;
            e->type = wl_vehicle_type(jstr(line, "type"));
            bool dup = false;
            for (int k = 0; k < n && !dup; k++) {
                dup = strcmp(out[k].line, e->line) == 0 && out[k].dir == e->dir && out[k].rbl == e->rbl;
            }
            if (!dup) n++;
        }
    }
    cJSON_Delete(root);
    return n;
}

typedef struct {
    wl_station_t st;
    int rank;
} ranked_t;

int wl_search_stations(const char *csv, const char *query, wl_station_t *out, int max)
{
    char q[48];
    tx_fold(query, q, sizeof(q));
    tx_trim(q);
    if (!q[0] || max <= 0) return 0;

    ranked_t *hits = calloc((size_t)max, sizeof(*hits));
    if (!hits) return 0;
    int n = 0;
    const char *p = csv;
    bool header = true;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (header) {
            header = false;
        } else {
            // DIVA;PlatformText;Municipality;...
            const char *semi = memchr(p, ';', len);
            const char *semi2 = semi ? memchr(semi + 1, ';', len - (size_t)(semi + 1 - p)) : NULL;
            if (semi && semi2) {
                char name[48], folded[64];
                size_t nl = (size_t)(semi2 - semi - 1);
                if (nl >= sizeof(name)) nl = sizeof(name) - 1;
                memcpy(name, semi + 1, nl);
                name[nl] = 0;
                tx_fold(name, folded, sizeof(folded));
                const char *at = strstr(folded, q);
                if (at) {
                    int rank = (at == folded ? 0 : 1000) + (int)strlen(folded);
                    // Keep the `max` best: replace the worst hit when full.
                    int slot = n < max ? n++ : -1;
                    if (slot < 0) {
                        int worst = 0;
                        for (int i = 1; i < max; i++) {
                            if (hits[i].rank > hits[worst].rank) worst = i;
                        }
                        if (hits[worst].rank > rank) slot = worst;
                    }
                    if (slot >= 0) {
                        hits[slot].rank = rank;
                        hits[slot].st.diva = (int32_t)strtol(p, NULL, 10);
                        tx_copy(hits[slot].st.name, name, sizeof(hits[slot].st.name));
                    }
                }
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    // Insertion sort by rank (n is small).
    for (int i = 1; i < n; i++) {
        ranked_t t = hits[i];
        int k = i - 1;
        while (k >= 0 && hits[k].rank > t.rank) {
            hits[k + 1] = hits[k];
            k--;
        }
        hits[k + 1] = t;
    }
    for (int i = 0; i < n; i++) {
        out[i] = hits[i].st;
    }
    free(hits);
    return n;
}
