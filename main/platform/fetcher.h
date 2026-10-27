// Background task that polls the configured stops and publishes the merged
// departure board (app_set_board). Resilience rules:
//  * a stop that fails keeps its last good departures for STALE_KEEP_S,
//  * failures back off exponentially (15 s .. 5 min), rate limits back off more,
//  * a config change or a new WiFi connection triggers an immediate refetch.
#pragma once

void fetcher_start(void);
