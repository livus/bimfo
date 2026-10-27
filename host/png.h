#pragma once

#include <stdbool.h>
#include <stdint.h>

// Writes an 8-bit RGB PNG (uncompressed deflate, no dependencies).
bool png_write_rgb(const char *path, const uint8_t *rgb, int w, int h);
