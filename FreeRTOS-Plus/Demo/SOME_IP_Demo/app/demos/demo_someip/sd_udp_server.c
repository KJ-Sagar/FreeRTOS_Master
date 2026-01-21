#include "sd_udp_server.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"
#include <string.h>

/* ================= Configuration ================= */
#define SD_UDP_PORT 30490

/* ================= Services offered ================= */
static const uint16_t offered_services[] =
{
    0x1234, /* Heartbeat */
    0x1001, /* Sensor */
    0x1002  /* Engine */
};

static void sd_udp_task(void *arg)
{
    (void)arg;

    Socket_t sock;
    struct freertos_sockaddr local, remote;
    socklen_t remote_len = sizeof(remote);

    uint8_t rx_buf[1];
    uint8_t tx_buf[32];
    size_t tx_len;

    sock = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_DGRAM,
        FREERTOS_IPPROTO_UDP
    );

    configASSERT(sock != FREERTOS_INVALID_SOCKET);

    /* Bind to SD port */
    local.sin_port = FreeRTOS_htons(SD_UDP_PORT);
    local.sin_address.ulIP_IPv4 = FreeRTOS_GetIPAddress();

    FreeRTOS_bind(sock, &local, sizeof(local));

    FreeRTOS_printf((
        "SD: UDP unicast SD server listening on %u\r\n",
        SD_UDP_PORT
    ));

    for (;;)
    {
        /* Wait for FindService */
        if (FreeRTOS_recvfrom(
                sock,
                rx_buf,
                sizeof(rx_buf),
                0,
                &remote,
                &remote_len) <= 0)
        {
            continue;
        }

        FreeRTOS_printf((
            "SD: FindService from %lxip\r\n",
            FreeRTOS_ntohl(remote.sin_address.ulIP_IPv4)
        ));

        /* Build OfferService payload */
        tx_len = 0;
        for (size_t i = 0;
             i < (sizeof(offered_services) / sizeof(offered_services[0]));
             i++)
        {
            uint16_t sid = FreeRTOS_htons(offered_services[i]);
            memcpy(&tx_buf[tx_len], &sid, sizeof(sid));
            tx_len += sizeof(sid);
        }

        /* Unicast reply */
        FreeRTOS_sendto(
            sock,
            tx_buf,
            tx_len,
            0,
            &remote,
            remote_len
        );

        FreeRTOS_printf((
            "SD: Offer sent (%u bytes)\r\n",
            (unsigned)tx_len
        ));
    }
}

void sd_udp_server_start(void)
{
    xTaskCreate(
        sd_udp_task,
        "SD_UDP",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
}
