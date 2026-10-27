#pragma once

#include <stdint.h>

// Turns the DNS query in `buf` (len bytes, buffer capacity cap) into a reply
// in place that answers A queries with `ip` (network byte order) and other
// types with an empty answer. Returns the reply length, 0 to ignore the packet.
int dns_build_reply(uint8_t *buf, int len, int cap, uint32_t ip);
