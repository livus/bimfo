#include "dns_server.h"

#include <string.h>

#include "dns_reply.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

static const char *TAG = "dns";

static TaskHandle_t s_task;
static volatile bool s_run;
static uint32_t s_ip;   // network byte order

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (sock < 0 || bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "cannot bind port 53");
        if (sock >= 0) close(sock);
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    struct timeval tv = {.tv_sec = 1};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    uint8_t buf[512];
    while (s_run) {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) continue;
        int r = dns_build_reply(buf, n, sizeof(buf), s_ip);
        if (r > 0) {
            sendto(sock, buf, r, 0, (struct sockaddr *)&from, fl);
        }
    }
    close(sock);
    s_task = NULL;
    vTaskDelete(NULL);
}

void dns_server_start(const char *ip)
{
    s_ip = inet_addr(ip);
    s_run = true;  // also revives a task that was asked to stop but has not yet
    if (!s_task) {
        xTaskCreate(dns_task, "dns", 3072, NULL, 3, &s_task);
    }
}

void dns_server_stop(void)
{
    s_run = false;  // the task notices within its 1 s receive timeout
}
