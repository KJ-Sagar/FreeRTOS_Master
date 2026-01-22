#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include "someip_server_state.h"
#include "app/demos/demo_someip/someip_protocol.h"

#include <stdio.h>
#include <string.h>

#define SERVICE_HEARTBEAT  0x1234
#define METHOD_HEARTBEAT   0x0001
#define HEARTBEAT_IDLE_TIMEOUT_MS  2000

void someip_notify_task(void *arg)
{
    someip_client_ctx_t *client = (someip_client_ctx_t *)arg;
    Socket_t sock = client->socket;

    uint8_t tx_buf[64];

    printf("SOME/IP: Notify task started\r\n");

    while (client->active)
    {
        if (client->heartbeat_subscribed)
        {
            someip_header_t hdr;
            uint32_t alive = FreeRTOS_htonl(1);

            hdr.service_id        = SERVICE_HEARTBEAT;
            hdr.method_id         = METHOD_HEARTBEAT;
            hdr.client_id         = 0x0000;
            hdr.session_id        = 0x0000;
            hdr.protocol_version  = SOMEIP_PROTOCOL_VERSION;
            hdr.interface_version = SOMEIP_INTERFACE_VERSION;
            hdr.message_type      = SOMEIP_MSG_NOTIFICATION;
            hdr.return_code       = SOMEIP_RET_OK;
            hdr.length            = 12;

            someip_hton_header(&hdr);
            memcpy(tx_buf, &hdr, sizeof(hdr));
            memcpy(tx_buf + sizeof(hdr), &alive, sizeof(alive));

            FreeRTOS_send(
                sock,
                tx_buf,
                sizeof(hdr) + sizeof(alive),
                0
            );

            printf("SOME/IP: Heartbeat notification sent\r\n");

            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else
        {
            /* Sleep lightly while unsubscribed */
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    printf("SOME/IP: Notify task stopped\r\n");

    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
}
