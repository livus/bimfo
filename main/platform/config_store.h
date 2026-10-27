// Persists app_config_t in NVS as JSON (forward compatible: unknown keys are
// ignored, missing keys keep their defaults).
#pragma once

#include "app_config.h"
#include "esp_err.h"

void config_store_load(app_config_t *cfg);
esp_err_t config_store_save(const app_config_t *cfg);
esp_err_t config_store_erase(void);
