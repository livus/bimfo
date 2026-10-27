// bimfo_parse <wl|wllines|oebb|oebbsearch|wlsearch> <file> [query]
// Runs one of the firmware parsers on a payload file and prints the JSON the
// device would serve. Used by host/live_check.py on fresh API responses.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "api_json.h"
#include "board.h"

static char *load(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = 0;
    fclose(f);
    *len = got;
    return buf;
}

static int print(cJSON *o)
{
    char *s = cJSON_Print(o);
    puts(s);
    free(s);
    cJSON_Delete(o);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s wl|wllines|oebb|oebbsearch|wlsearch file [query]\n", argv[0]);
        return 2;
    }
    size_t len;
    char *data = load(argv[2], &len);
    if (!data) {
        fprintf(stderr, "cannot read %s\n", argv[2]);
        return 2;
    }
    static board_t b;
    board_clear(&b);
    const char *mode = argv[1];
    int64_t now = (int64_t)time(NULL);
    if (strcmp(mode, "wl") == 0) {
        int64_t server = 0;
        int code = 0;
        wl_status_t r = wl_parse_monitor(data, NULL, 0, &b, &server, &code);
        if (r != WL_OK) {
            fprintf(stderr, "wl_parse_monitor failed: %d (code %d)\n", r, code);
            return 1;
        }
        board_sort(&b);
        return print(api_board_json(&b, server ? server : now));
    }
    if (strcmp(mode, "wllines") == 0) {
        wl_line_info_t lines[64];
        int n = wl_list_lines(data, lines, 64);
        if (n < 0) return 1;
        return print(api_wl_lines_json(lines, n));
    }
    if (strcmp(mode, "oebb") == 0) {
        if (oebb_parse_board(data, NULL, 0, &b, b.title, sizeof(b.title)) < 0) {
            fprintf(stderr, "oebb_parse_board failed\n");
            return 1;
        }
        board_sort(&b);
        return print(api_board_json(&b, now));
    }
    if (strcmp(mode, "oebbsearch") == 0) {
        oebb_station_t st[16];
        int n = oebb_parse_search(data, st, 16);
        if (n < 0) return 1;
        return print(api_oebb_stations_json(st, n));
    }
    if (strcmp(mode, "wlsearch") == 0 && argc > 3) {
        wl_station_t st[16];
        int n = wl_search_stations(data, argv[3], st, 16);
        return print(api_wl_stations_json(st, n));
    }
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}
