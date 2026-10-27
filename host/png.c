#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        crc_table[n] = c;
    }
}

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    c ^= 0xFFFFFFFFu;
    while (n--) {
        c = crc_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    put32(hdr, len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    fwrite(data, 1, len, f);
    uint32_t c = crc(0, hdr + 4, 4);
    c = crc(c, data, len);
    uint8_t tail[4];
    put32(tail, c);
    fwrite(tail, 1, 4, f);
}

bool png_write_rgb(const char *path, const uint8_t *rgb, int w, int h)
{
    crc_init();
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13];
    put32(ihdr, (uint32_t)w);
    put32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 2;   // RGB
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);

    // Raw scanlines (filter byte 0) wrapped in stored deflate blocks.
    size_t raw_len = (size_t)h * (1 + (size_t)w * 3);
    uint8_t *raw = malloc(raw_len);
    for (int y = 0; y < h; y++) {
        uint8_t *row = raw + (size_t)y * (1 + (size_t)w * 3);
        row[0] = 0;
        memcpy(row + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *z = malloc(z_len), *p = z;
    *p++ = 0x78;
    *p++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw_len; off += 65535) {
        size_t n = raw_len - off < 65535 ? raw_len - off : 65535;
        *p++ = off + n >= raw_len ? 1 : 0;
        *p++ = (uint8_t)n;
        *p++ = (uint8_t)(n >> 8);
        *p++ = (uint8_t)~n;
        *p++ = (uint8_t)(~n >> 8);
        memcpy(p, raw + off, n);
        p += n;
        for (size_t i = 0; i < n; i++) {
            a = (a + raw[off + i]) % 65521;
            b = (b + a) % 65521;
        }
    }
    put32(p, (b << 16) | a);
    chunk(f, "IDAT", z, (uint32_t)z_len);
    chunk(f, "IEND", NULL, 0);
    free(raw);
    free(z);
    return fclose(f) == 0;
}
