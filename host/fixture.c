#include "fixture.h"

#include <stdio.h>
#include <stdlib.h>

char *fixture_load(const char *name, size_t *len)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FIXTURE_DIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open fixture %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "short read %s\n", path);
        exit(2);
    }
    buf[n] = 0;
    fclose(f);
    if (len) {
        *len = (size_t)n;
    }
    return buf;
}
