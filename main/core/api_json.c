#include "api_json.h"

#include <stdlib.h>
#include <string.h>

const char *api_vehicle_type_name(uint8_t type)
{
    static const char *const names[] = {"unknown", "metro", "tram", "bus", "nightbus", "sbahn", "train"};
    return type < sizeof(names) / sizeof(names[0]) ? names[type] : names[0];
}

cJSON *api_board_json(const board_t *b, int64_t now)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "title", b->title);
    cJSON_AddStringToObject(root, "notice", b->notice);
    cJSON_AddNumberToObject(root, "fetched_at", (double)b->fetched_at);
    cJSON_AddNumberToObject(root, "now", (double)now);
    cJSON *arr = cJSON_AddArrayToObject(root, "departures");
    for (int i = 0; i < b->count; i++) {
        const departure_t *d = &b->dep[i];
        cJSON *o = cJSON_CreateObject();
        char dir[2] = {d->dir, 0};
        cJSON_AddStringToObject(o, "line", d->line);
        cJSON_AddStringToObject(o, "towards", d->towards);
        cJSON_AddStringToObject(o, "platform", d->platform);
        cJSON_AddStringToObject(o, "dir", dir);
        cJSON_AddStringToObject(o, "type", api_vehicle_type_name(d->type));
        cJSON_AddNumberToObject(o, "minutes", dep_minutes(d, now));
        cJSON_AddNumberToObject(o, "planned", (double)d->planned);
        cJSON_AddNumberToObject(o, "expected", (double)d->expected);
        cJSON_AddBoolToObject(o, "delayed", dep_is_delayed(d));
        cJSON_AddBoolToObject(o, "cancelled", (d->flags & DEP_F_CANCELLED) != 0);
        cJSON_AddBoolToObject(o, "barrier_free", (d->flags & DEP_F_BARRIER_FREE) != 0);
        cJSON_AddBoolToObject(o, "realtime", (d->flags & DEP_F_REALTIME) != 0);
        cJSON_AddNumberToObject(o, "stop", d->stop);
        cJSON_AddItemToArray(arr, o);
    }
    return root;
}

// Natural order for line names: "2" < "13A" < "U1" (numbers compared by value).
static int cmp_line_name(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            long na = strtol(a, (char **)&a, 10), nb = strtol(b, (char **)&b, 10);
            if (na != nb) return na < nb ? -1 : 1;
            continue;
        }
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int cmp_lines(const void *pa, const void *pb)
{
    const wl_line_info_t *a = pa, *b = pb;
    if (a->type != b->type) return a->type - b->type;
    int c = cmp_line_name(a->line, b->line);
    if (c) return c;
    return a->dir - b->dir;
}

cJSON *api_wl_lines_json(wl_line_info_t *lines, int n)
{
    qsort(lines, (size_t)n, sizeof(lines[0]), cmp_lines);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *o = cJSON_CreateObject();
        char dir[2] = {lines[i].dir, 0};
        cJSON_AddStringToObject(o, "line", lines[i].line);
        cJSON_AddStringToObject(o, "towards", lines[i].towards);
        cJSON_AddStringToObject(o, "dir", dir);
        cJSON_AddNumberToObject(o, "rbl", lines[i].rbl);
        cJSON_AddStringToObject(o, "type", api_vehicle_type_name(lines[i].type));
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

cJSON *api_wl_stations_json(const wl_station_t *st, int n)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", st[i].diva);
        cJSON_AddStringToObject(o, "name", st[i].name);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

cJSON *api_oebb_stations_json(const oebb_station_t *st, int n)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", st[i].eva);
        cJSON_AddStringToObject(o, "name", st[i].name);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}
