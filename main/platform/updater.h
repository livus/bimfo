// Firmware updates from the GitHub releases (CONFIG_APP_UPDATE_URL): nightly
// when enabled on the settings page, or on request. Also the lock that keeps
// an update apart from a firmware upload on the settings page.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool available;         // the running firmware is signed, so updates can be verified
    bool busy;
    int progress;           // 0..100 while downloading, else -1
    bool restarting;        // installed, restarting into the new version
    int64_t last_check;     // unix time, 0 = not yet
    int64_t next_check;     // next automatic check, 0 = off or clock unknown
    char message[112];      // result of the last check
} update_status_t;

void updater_start(void);

// Checks now and installs a newer version. NULL if the check started, else
// why it did not.
const char *updater_check_now(void);

void updater_get_status(update_status_t *out);

// True if the running firmware carries a signature: only then does it accept
// updates (uploaded or downloaded), signed with the same key.
bool updater_signed(void);

// One firmware write at a time. Not recursive; any task may unlock.
bool updater_lock(void);
void updater_unlock(void);
