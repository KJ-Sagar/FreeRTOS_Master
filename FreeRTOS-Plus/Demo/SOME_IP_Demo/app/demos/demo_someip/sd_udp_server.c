#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#define SD_UDP_PORT 30490
#define SD_BUF_SIZE 128

static void sd_udp_task(void *arg)
{
    (void)arg;

    Socket_t sock;
    struct freertos_sockaddr bind_addr;
    struct freertos_sockaddr from_addr;
    socklen_t from_len;
    uint8_t rx_buf[SD_BUF_SIZE];

    /* Create UDP socket */
    sock = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_DGRAM,
        FREERTOS_IPPROTO_UDP
    );

    configASSERT(sock != FREERTOS_INVALID_SOCKET);

    /* Bind to 0.0.0.0:SD_UDP_PORT */
    bind_addr.sin_port = FreeRTOS_htons(SD_UDP_PORT);
    bind_addr.sin_address.ulIP_IPv4 = 0;   /* ANY */

    configASSERT(
        FreeRTOS_bind(sock, &bind_addr, sizeof(bind_addr)) == 0
    );

    FreeRTOS_printf(("SD: UDP listening on %u\r\n", SD_UDP_PORT));

    for (;;)
    {
        from_len = sizeof(from_addr);

        int rx_len = FreeRTOS_recvfrom(
            sock,
            rx_buf,
            sizeof(rx_buf),
            0,
            &from_addr,
            &from_len
        );

        if (rx_len <= 0)
        {
            continue;
        }

        FreeRTOS_printf((
            "SD: request from %lx:%u\r\n",
            FreeRTOS_ntohl(from_addr.sin_address.ulIP_IPv4),
            FreeRTOS_ntohs(from_addr.sin_port)
        ));

        /* Simple unicast SD response: list of service IDs */
        uint16_t services[] = {
            FreeRTOS_htons(0x1234), /* Heartbeat */
            FreeRTOS_htons(0x1001), /* Sensor */
            FreeRTOS_htons(0x1002)  /* Engine */
        };

        FreeRTOS_sendto(
            sock,
            services,
            sizeof(services),
            0,
            &from_addr,
            from_len
        );
    }
}

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
