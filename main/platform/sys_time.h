#pragma once

// Starts SNTP once (safe to call on every new IP); sets APP_EV_TIME_SYNCED.
void sys_time_start(void);
