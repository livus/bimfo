#include "dns_reply.h"

#include <string.h>

int dns_build_reply(uint8_t *buf, int len, int cap, uint32_t ip)
{
    if (len < 12 || len > cap) return 0;
    uint16_t qdcount = (uint16_t)(buf[4] << 8 | buf[5]);
    if ((buf[2] & 0x80) || qdcount != 1) return 0;  // only plain single queries

    // Walk the question name (labels, no compression in queries).
    int p = 12;
    while (p < len && buf[p]) {
        if (buf[p] & 0xC0) return 0;
        p += buf[p] + 1;
    }
    p++;
    if (p + 4 > len) return 0;
    uint16_t qtype = (uint16_t)(buf[p] << 8 | buf[p + 1]);
    p += 4;

    buf[2] = 0x81;  // response, recursion desired
    buf[3] = 0x80;  // recursion available, no error
    buf[8] = buf[9] = buf[10] = buf[11] = 0;  // no authority / additional records
    if (qtype != 1 /* A */) {
        buf[6] = buf[7] = 0;  // answer nothing for AAAA & co.
        return p;
    }
    static const uint8_t rr[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4};
    if (p + (int)sizeof(rr) + 4 > cap) return 0;
    buf[6] = 0;
    buf[7] = 1;
    memcpy(buf + p, rr, sizeof(rr));
    memcpy(buf + p + sizeof(rr), &ip, 4);
    return p + (int)sizeof(rr) + 4;
}
