// Configuration web page and JSON API (see main/www/index.html):
//   GET  /                     page
//   GET  /api/status           device, WiFi and fetch status
//   GET  /api/config           configuration (WiFi password omitted)
//   POST /api/config           save configuration (restarts on WiFi/hostname change)
//   GET  /api/board            current departures
//   GET  /api/wifi/scan        visible networks
//   GET  /api/wl/search?q=     Wiener Linien stations
//   GET  /api/wl/lines?id=     lines/directions/platforms of a WL station (live)
//   GET  /api/oebb/search?q=   OEBB stations
//   POST /api/ota              firmware upload (raw .bin body)
//   POST /api/reboot, /api/reset (factory reset), /api/setup (open setup AP)
// Unknown URLs redirect to the page while the setup access point is active
// (captive portal).
#pragma once

void web_server_start(void);
