// Captive-portal DNS: answers every A query with the access point address so
// phones show the setup page right after joining the network.
#pragma once

void dns_server_start(const char *ip);
void dns_server_stop(void);
