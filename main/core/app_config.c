#include "app_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "textutil.h"
#include "timeutil.h"

static const char *const THEME_NAMES[THEME_COUNT] = {"wl", "compact", "oebb"};

const char *cfg_theme_name(uint8_t theme)
{
    return theme < THEME_COUNT ? THEME_NAMES[theme] : THEME_NAMES[0];
}

void cfg_defaults(app_config_t *c)
{
    memset(c, 0, sizeof(*c));
    strcpy(c->hostname, "bimfo");
    c->theme = THEME_WL;
    c->group_lines = true;
    c->wl_paging = true;
    c->wl_step_free = true;
    c->brightness = 80;
    c->night_brightness = 15;
    c->night_start = 22 * 60;
    c->night_end = 6 * 60;
    c->refresh_s = 30;
}

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void get_str(const cJSON *obj, const char *key, char *dst, size_t n)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(it) && it->valuestring) {
        tx_copy(dst, it->valuestring, n);
    }
}

static void get_int(const cJSON *obj, const char *key, int lo, int hi, void *dst, size_t size)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(it)) {
        return;
    }
    int v = clamp_int(it->valueint, lo, hi);
    if (size == 1) *(uint8_t *)dst = (uint8_t)v;
    else if (size == 2) *(uint16_t *)dst = (uint16_t)v;
    else *(int32_t *)dst = v;
}
#define GET_INT(obj, key, lo, hi, field) get_int(obj, key, lo, hi, &(field), sizeof(field))

static void get_bool(const cJSON *obj, const char *key, bool *dst)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(it)) {
        *dst = cJSON_IsTrue(it);
    }
}

// "HH:MM" -> minutes after midnight
static void get_hhmm(const cJSON *obj, const char *key, uint16_t *dst)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    int h = -1, m = -1;
    if (cJSON_IsString(it) && sscanf(it->valuestring, "%d:%d", &h, &m) == 2 &&
        h >= 0 && h < 24 && m >= 0 && m < 60) {
        *dst = (uint16_t)(h * 60 + m);
    }
}

static void sanitize_hostname(char *h)
{
    char *w = h;
    for (char *r = h; *r; r++) {
        char ch = (char)tolower((unsigned char)*r);
        if (isalnum((unsigned char)ch) || (ch == '-' && w != h)) {
            *w++ = ch;
        }
    }
    *w = 0;
    if (!h[0]) {
        strcpy(h, "bimfo");
    }
}

static void parse_stop(const cJSON *js, cfg_stop_t *s)
{
    memset(s, 0, sizeof(*s));
    char kind[8] = "wl";
    get_str(js, "kind", kind, sizeof(kind));
    s->kind = strcmp(kind, "oebb") == 0 ? STOP_OEBB : STOP_WL;
    get_str(js, "name", s->name, sizeof(s->name));
    GET_INT(js, "id", 0, 0x7fffffff, s->id);
    GET_INT(js, "walk", 0, 60, s->walk_min);
    get_bool(js, "trains_only", &s->trains_only);
    const cJSON *sel = cJSON_GetObjectItemCaseSensitive(js, "sel");
    const cJSON *it;
    cJSON_ArrayForEach(it, sel) {
        if (s->nsel >= CFG_MAX_SEL) break;
        cfg_sel_t *e = &s->sel[s->nsel];
        get_str(it, "line", e->line, sizeof(e->line));
        if (!e->line[0]) continue;
        char dir[4] = "";
        get_str(it, "dir", dir, sizeof(dir));
        e->dir = (dir[0] == 'H' || dir[0] == 'R') ? dir[0] : 0;
        GET_INT(it, "rbl", 0, 0x7fffffff, e->rbl);
        s->nsel++;
    }
}

bool cfg_from_json(app_config_t *c, const char *json, char *err, size_t errlen)
{
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsObject(root)) {
        snprintf(err, errlen, "invalid JSON");
        cJSON_Delete(root);
        return false;
    }
    app_config_t n = *c;

    const cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    if (cJSON_IsObject(wifi)) {
        get_str(wifi, "ssid", n.wifi_ssid, sizeof(n.wifi_ssid));
        const cJSON *pass = cJSON_GetObjectItemCaseSensitive(wifi, "pass");
        if (cJSON_IsString(pass) && pass->valuestring[0]) {
            tx_copy(n.wifi_pass, pass->valuestring, sizeof(n.wifi_pass));
        }
        if (!n.wifi_ssid[0]) {
            n.wifi_pass[0] = 0;
        }
    }
    get_str(root, "hostname", n.hostname, sizeof(n.hostname));
    sanitize_hostname(n.hostname);

    char theme[8] = "";
    get_str(root, "theme", theme, sizeof(theme));
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme, THEME_NAMES[i]) == 0) n.theme = (uint8_t)i;
    }
    get_bool(root, "group", &n.group_lines);
    get_bool(root, "wl_notice", &n.wl_notice);
    get_bool(root, "wl_paging", &n.wl_paging);
    get_bool(root, "wl_step_free", &n.wl_step_free);
    GET_INT(root, "brightness", 0, 100, n.brightness);
    GET_INT(root, "refresh", 15, 600, n.refresh_s);
    get_bool(root, "flip", &n.flip);
    get_bool(root, "auto_update", &n.auto_update);

    const cJSON *night = cJSON_GetObjectItemCaseSensitive(root, "night");
    if (cJSON_IsObject(night)) {
        GET_INT(night, "brightness", 0, 100, n.night_brightness);
        get_hhmm(night, "start", &n.night_start);
        get_hhmm(night, "end", &n.night_end);
    }

    const cJSON *stops = cJSON_GetObjectItemCaseSensitive(root, "stops");
    if (cJSON_IsArray(stops)) {
        n.nstops = 0;
        const cJSON *it;
        cJSON_ArrayForEach(it, stops) {
            if (n.nstops >= CFG_MAX_STOPS) break;
            cfg_stop_t s;
            parse_stop(it, &s);
            if (s.id > 0) {
                n.stops[n.nstops++] = s;
            }
        }
    }
    cJSON_Delete(root);
    *c = n;
    return true;
}

char *cfg_to_json(const app_config_t *c, bool include_secrets)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddStringToObject(wifi, "ssid", c->wifi_ssid);
    if (include_secrets) {
        cJSON_AddStringToObject(wifi, "pass", c->wifi_pass);
    } else {
        cJSON_AddBoolToObject(wifi, "has_pass", c->wifi_pass[0] != 0);
    }
    cJSON_AddStringToObject(root, "hostname", c->hostname);
    cJSON_AddStringToObject(root, "theme", cfg_theme_name(c->theme));
    cJSON_AddBoolToObject(root, "group", c->group_lines);
    cJSON_AddBoolToObject(root, "wl_notice", c->wl_notice);
    cJSON_AddBoolToObject(root, "wl_paging", c->wl_paging);
    cJSON_AddBoolToObject(root, "wl_step_free", c->wl_step_free);
    cJSON_AddNumberToObject(root, "brightness", c->brightness);
    cJSON_AddNumberToObject(root, "refresh", c->refresh_s);
    cJSON_AddBoolToObject(root, "flip", c->flip);
    cJSON_AddBoolToObject(root, "auto_update", c->auto_update);

    char hhmm[8];
    cJSON *night = cJSON_AddObjectToObject(root, "night");
    cJSON_AddNumberToObject(night, "brightness", c->night_brightness);
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", c->night_start / 60, c->night_start % 60);
    cJSON_AddStringToObject(night, "start", hhmm);
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", c->night_end / 60, c->night_end % 60);
    cJSON_AddStringToObject(night, "end", hhmm);

    cJSON *stops = cJSON_AddArrayToObject(root, "stops");
    for (int i = 0; i < c->nstops; i++) {
        const cfg_stop_t *s = &c->stops[i];
        cJSON *js = cJSON_CreateObject();
        cJSON_AddStringToObject(js, "kind", s->kind == STOP_OEBB ? "oebb" : "wl");
        cJSON_AddStringToObject(js, "name", s->name);
        cJSON_AddNumberToObject(js, "id", s->id);
        cJSON_AddNumberToObject(js, "walk", s->walk_min);
        if (s->kind == STOP_OEBB) {
            cJSON_AddBoolToObject(js, "trains_only", s->trains_only);
        }
        cJSON *sel = cJSON_AddArrayToObject(js, "sel");
        for (int k = 0; k < s->nsel; k++) {
            cJSON *e = cJSON_CreateObject();
            cJSON_AddStringToObject(e, "line", s->sel[k].line);
            if (s->sel[k].dir) {
                char dir[2] = {s->sel[k].dir, 0};
                cJSON_AddStringToObject(e, "dir", dir);
            }
            if (s->sel[k].rbl) {
                cJSON_AddNumberToObject(e, "rbl", s->sel[k].rbl);
            }
            cJSON_AddItemToArray(sel, e);
        }
        cJSON_AddItemToArray(stops, js);
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

int cfg_brightness(const app_config_t *c, int64_t now)
{
    if (now <= 0 || c->night_start == c->night_end) {
        return c->brightness;
    }
    tu_fields_t f;
    tu_vienna_fields(now, &f);
    int m = f.hour * 60 + f.min;
    bool night = c->night_start < c->night_end ? (m >= c->night_start && m < c->night_end)
                                               : (m >= c->night_start || m < c->night_end);
    return night ? c->night_brightness : c->brightness;
}

void cfg_board_title(const app_config_t *c, char *buf, size_t n)
{
    if (n == 0) {
        return;
    }
    buf[0] = 0;
    char tmp[256] = "";
    size_t len = 0;
    for (int i = 0; i < c->nstops; i++) {
        len += (size_t)snprintf(tmp + len, sizeof(tmp) - len, "%s%s", i ? " \xC2\xB7 " : "", c->stops[i].name);
        if (len >= sizeof(tmp)) {
            break;
        }
    }
    tx_copy(buf, tmp, n);
}

bool cfg_stop_accepts(const cfg_stop_t *s, const char *line, char dir)
{
    if (s->nsel == 0) {
        return true;
    }
    for (int i = 0; i < s->nsel; i++) {
        if (strcmp(s->sel[i].line, line) == 0 && (!s->sel[i].dir || !dir || s->sel[i].dir == dir)) {
            return true;
        }
    }
    return false;
}
