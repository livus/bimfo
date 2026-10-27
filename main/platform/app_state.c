#include "app_state.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "config_store.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "app";

// 2024-01-01: anything earlier means the RTC was never set.
#define CLOCK_VALID_AFTER 1704067200

static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
EXT_RAM_BSS_ATTR static app_config_t s_config;
EXT_RAM_BSS_ATTR static board_t s_board;
static uint32_t s_board_gen;
static net_status_t s_net;
static fetch_status_t s_fetch;

#define MAX_STACKS 8
static struct {
    char name[16];
    uint32_t min_free;
} s_stacks[MAX_STACKS];
static int s_nstacks;

#define LOCK() xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

void app_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    config_store_load(&s_config);
    board_clear(&s_board);
}

EventGroupHandle_t app_events(void)
{
    return s_events;
}

void app_get_config(app_config_t *out)
{
    LOCK();
    *out = s_config;
    UNLOCK();
}

esp_err_t app_set_config(const app_config_t *cfg)
{
    esp_err_t err = config_store_save(cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saving config failed: %s", esp_err_to_name(err));
        return err;
    }
    LOCK();
    // Display-only changes (theme, brightness, ...) need no new data.
    bool refetch = s_config.nstops != cfg->nstops || s_config.refresh_s != cfg->refresh_s ||
                   memcmp(s_config.stops, cfg->stops, sizeof(cfg->stops)) != 0;
    s_config = *cfg;
    UNLOCK();
    if (refetch) {
        xEventGroupSetBits(s_events, APP_EV_CONFIG_CHANGED);
    }
    return ESP_OK;
}

void app_get_board(board_t *out, uint32_t *generation)
{
    LOCK();
    *out = s_board;
    if (generation) {
        *generation = s_board_gen;
    }
    UNLOCK();
}

uint32_t app_board_generation(void)
{
    LOCK();
    uint32_t g = s_board_gen;
    UNLOCK();
    return g;
}

void app_set_board(const board_t *b)
{
    LOCK();
    s_board = *b;
    s_board_gen++;
    UNLOCK();
}

void app_get_net(net_status_t *out)
{
    LOCK();
    *out = s_net;
    UNLOCK();
}

void app_update_net(void (*fn)(net_status_t *net, void *ctx), void *ctx)
{
    LOCK();
    fn(&s_net, ctx);
    UNLOCK();
}

void app_get_fetch(fetch_status_t *out)
{
    LOCK();
    *out = s_fetch;
    UNLOCK();
}

void app_set_fetch(const fetch_status_t *f)
{
    LOCK();
    s_fetch = *f;
    UNLOCK();
}

void app_note_stack(void)
{
    const char *name = pcTaskGetName(NULL);
    uint32_t free_bytes = (uint32_t)uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    LOCK();
    int i = 0;
    while (i < s_nstacks && strcmp(s_stacks[i].name, name) != 0) i++;
    if (i == s_nstacks && s_nstacks < MAX_STACKS) {
        strlcpy(s_stacks[i].name, name, sizeof(s_stacks[i].name));
        s_stacks[i].min_free = UINT32_MAX;
        s_nstacks++;
    }
    if (i < s_nstacks && free_bytes < s_stacks[i].min_free) {
        s_stacks[i].min_free = free_bytes;
    }
    UNLOCK();
}

int app_stack_report(char names[][16], uint32_t *free_bytes, int max)
{
    LOCK();
    int n = s_nstacks < max ? s_nstacks : max;
    for (int i = 0; i < n; i++) {
        strlcpy(names[i], s_stacks[i].name, 16);
        free_bytes[i] = s_stacks[i].min_free;
    }
    UNLOCK();
    return n;
}

int64_t app_now(void)
{
    time_t t = time(NULL);
    return t > CLOCK_VALID_AFTER ? (int64_t)t : 0;
}

void app_set_time_from_server(int64_t unix_time)
{
    // SNTP wins once it has synced; this only bridges the time until then.
    if (unix_time <= CLOCK_VALID_AFTER || (xEventGroupGetBits(s_events) & APP_EV_TIME_SYNCED)) {
        return;
    }
    time_t now = time(NULL);
    if (now <= CLOCK_VALID_AFTER || now - unix_time > 5 || unix_time - now > 5) {
        struct timeval tv = {.tv_sec = (time_t)unix_time};
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "clock set from API server time");
    }
}
