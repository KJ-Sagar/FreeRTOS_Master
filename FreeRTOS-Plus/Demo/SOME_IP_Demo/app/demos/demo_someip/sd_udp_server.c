#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#include <string.h>
#include <stdint.h>

/* SD listens on SOME/IP-SD well-known port */
#define SD_UDP_PORT 30490

static void sd_udp_task(void *arg);

void sd_udp_server_start(void)
{
    xTaskCreate(
        sd_udp_task,
        "SD_UDP",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
}

static void sd_udp_task(void *arg)
{
    (void)arg;

    Socket_t sock;
    struct freertos_sockaddr local_addr;
    struct freertos_sockaddr from;
    socklen_t from_len = sizeof(from);

    uint8_t rx_buf[64];

    sock = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_DGRAM,
        FREERTOS_IPPROTO_UDP
    );

    configASSERT(sock != FREERTOS_INVALID_SOCKET);

    local_addr.sin_port = FreeRTOS_htons(SD_UDP_PORT);
    FreeRTOS_bind(sock, &local_addr, sizeof(local_addr));

    FreeRTOS_printf(("SD: UDP listening on %u\r\n", SD_UDP_PORT));

    for (;;)
    {
        int len = FreeRTOS_recvfrom(
            sock,
            rx_buf,
            sizeof(rx_buf),
            0,
            &from,
            &from_len
        );

        /* IMPORTANT: ignore timeouts / errors */
        if (len <= 0)
        {
            continue;
        }

        FreeRTOS_printf(("SD: Request received (%d bytes)\r\n", len));

        /* ---- Send service list response ----
         * Format: simple list of uint16_t service IDs
         */
        uint16_t services[] =
        {
            FreeRTOS_htons(0x1234), /* Heartbeat */
            FreeRTOS_htons(0x1001), /* Sensor */
            FreeRTOS_htons(0x1002)  /* Engine */
        };

        FreeRTOS_sendto(
            sock,
            services,
            sizeof(services),
            0,
            &from,
            from_len
        );
    }
}
