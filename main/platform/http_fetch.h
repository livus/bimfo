// Small HTTPS GET helper: whole response body into a NUL-terminated buffer
// (PSRAM when available), certificate bundle, redirects, size limit.
#pragma once

#include <stddef.h>

#include "esp_err.h"

typedef struct {
    char *data;     // NUL-terminated, free with http_buf_free()
    size_t len;
    int status;     // HTTP status code (0 if no response)
} http_buf_t;

// Returns ESP_OK only for a complete 2xx response. On failure `out` holds
// whatever arrived (status may still be set) and must be freed as well.
esp_err_t http_get(const char *url, size_t max_len, int timeout_ms, http_buf_t *out);
void http_buf_free(http_buf_t *b);

// Human-readable description of the last failure for status displays.
const char *http_strerror(esp_err_t err, const http_buf_t *b, char *buf, size_t n);
