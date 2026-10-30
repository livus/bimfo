#include "textutil.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void tx_copy(char *dst, const char *src, size_t n)
{
    if (n == 0) {
        return;
    }
    if (!src) {
        dst[0] = 0;
        return;
    }
    size_t len = strlen(src);
    if (len >= n) {
        len = n - 1;
        // Back off over UTF-8 continuation bytes so we never cut a character.
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    memcpy(dst, src, len);
    dst[len] = 0;
}

int tx_utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp < 0x110000) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

void tx_html_decode(char *s)
{
    static const struct {
        const char *name;
        char ch;
    } named[] = {
        {"amp;", '&'}, {"lt;", '<'}, {"gt;", '>'}, {"quot;", '"'}, {"apos;", '\''},
    };
    char *r = s, *w = s;
    while (*r) {
        if (*r != '&') {
            *w++ = *r++;
            continue;
        }
        bool done = false;
        if (r[1] == '#') {
            char *end;
            bool hex = r[2] == 'x' || r[2] == 'X';
            const char *digits = r + (hex ? 3 : 2);
            unsigned long cp = strtoul(digits, &end, hex ? 16 : 10);
            if (*end == ';' && end > digits && cp > 0) {
                if (cp != 0x200B) {  // zero-width space
                    // Writing never overtakes reading: an entity is >= 4 bytes.
                    w += tx_utf8_encode((uint32_t)cp, w);
                }
                r = end + 1;
                done = true;
            }
        } else {
            for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
                size_t l = strlen(named[i].name);
                if (strncmp(r + 1, named[i].name, l) == 0) {
                    *w++ = named[i].ch;
                    r += 1 + l;
                    done = true;
                    break;
                }
            }
        }
        if (!done) {
            *w++ = *r++;
        }
    }
    *w = 0;
}

bool tx_is_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        int extra;
        if (*p < 0x80) extra = 0;
        else if ((*p & 0xE0) == 0xC0 && *p >= 0xC2) extra = 1;
        else if ((*p & 0xF0) == 0xE0) extra = 2;
        else if ((*p & 0xF8) == 0xF0 && *p <= 0xF4) extra = 3;
        else return false;
        p++;
        for (int i = 0; i < extra; i++, p++) {
            if ((*p & 0xC0) != 0x80) return false;
        }
    }
    return true;
}

char *tx_latin1_to_utf8(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        n += *p < 0x80 ? 1 : 2;
    }
    char *out = malloc(n + 1);
    if (!out) return NULL;
    char *w = out;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        w += tx_utf8_encode(*p, w);
    }
    *w = 0;
    return out;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void tx_url_decode(char *s)
{
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '+') {
            *w++ = ' ';
        } else if (*r == '%' && hex_val(r[1]) >= 0 && hex_val(r[2]) >= 0) {
            char c = (char)(hex_val(r[1]) << 4 | hex_val(r[2]));
            if (c) {
                *w++ = c;
            }
            r += 2;
        } else {
            *w++ = *r;
        }
    }
    *w = 0;
}

void tx_trim(char *s)
{
    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1])) {
        s[--len] = 0;
    }
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) {
        start++;
    }
    if (start) {
        memmove(s, s + start, len - start + 1);
    }
}

bool tx_strip_suffix(char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    if (ls >= lx && strcmp(s + ls - lx, suffix) == 0) {
        s[ls - lx] = 0;
        return true;
    }
    return false;
}

void tx_split_transfers(const char *towards, char *name, size_t n, char *marks, size_t mn)
{
    char buf[96];   // destinations are at most 48 bytes
    size_t o = 0, m = 0;
    buf[0] = 0;
    marks[0] = 0;
    for (const char *p = towards; *p;) {
        while (*p == ' ') p++;
        const char *w = p;
        while (*p && *p != ' ') p++;
        size_t len = (size_t)(p - w);
        if (len == 0) break;
        bool comma = w[len - 1] == ',';
        if (len - comma == 1 && (w[0] == 'U' || w[0] == 'S')) {
            if (m + 1 < mn && !strchr(marks, w[0])) {
                marks[m++] = w[0];
                marks[m] = 0;
            }
            // "Stadthalle U, X": the comma stays with the name.
            if (comma && o > 0 && buf[o - 1] != ',' && o + 1 < sizeof(buf)) {
                buf[o++] = ',';
                buf[o] = 0;
            }
            continue;
        }
        if (o + 1 + len >= sizeof(buf)) break;
        if (o) buf[o++] = ' ';
        memcpy(buf + o, w, len);
        o += len;
        buf[o] = 0;
    }
    while (o && buf[o - 1] == ',') buf[--o] = 0;
    tx_copy(name, buf, n);
}

// Maps the Latin-1 supplement (U+00C0..U+00FF) to plain lowercase ASCII.
static const char *fold_latin1(uint32_t cp)
{
    static const char *tbl[64] = {
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", "x", "o", "u", "u", "u", "u", "y", "th", "ss",
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u", "u", "y", "th", "y",
    };
    return (cp >= 0xC0 && cp <= 0xFF) ? tbl[cp - 0xC0] : NULL;
}

void tx_fold(const char *in, char *out, size_t n)
{
    size_t w = 0;
    const unsigned char *p = (const unsigned char *)in;
    while (*p && w + 1 < n) {
        uint32_t cp;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && p[1]) {
            cp = ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else {
            // Skip anything outside Latin-1; it never matters for stop names.
            p++;
            while ((*p & 0xC0) == 0x80) p++;
            continue;
        }
        const char *rep = fold_latin1(cp);
        if (rep) {
            for (; *rep && w + 1 < n; rep++) out[w++] = *rep;
        } else if (cp < 0x80) {
            out[w++] = (char)tolower((int)cp);
        }
    }
    out[w] = 0;
}
