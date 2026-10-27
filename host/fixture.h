#pragma once

#include <stddef.h>

// Reads host/fixtures/<name> into a NUL-terminated malloc'd buffer (exits on error).
char *fixture_load(const char *name, size_t *len);
