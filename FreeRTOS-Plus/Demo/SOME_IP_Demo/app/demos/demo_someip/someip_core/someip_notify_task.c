#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "someip_server_state.h"
#include "someip_protocol.h"

void someip_notify_task(void *arg)
{
    (void)arg;
    uint8_t tx_buf[64];

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));

        for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
        {
            if (!g_someip_clients[i].active ||
                !g_someip_clients[i].heartbeat_subscribed)
                continue;

            someip_header_t hdr;
            uint32_t alive = FreeRTOS_htonl(1);

            hdr.service_id        = SERVICE_HEARTBEAT;
            hdr.method_id         = METHOD_HEARTBEAT;
            hdr.client_id         = 0;
            hdr.session_id        = 0;
            hdr.protocol_version  = SOMEIP_PROTOCOL_VERSION;
            hdr.interface_version = SOMEIP_INTERFACE_VERSION;
            hdr.message_type      = SOMEIP_MSG_NOTIFICATION;
            hdr.return_code       = SOMEIP_RET_OK;
            hdr.length            = 12;

            someip_hton_header(&hdr);
            memcpy(tx_buf, &hdr, sizeof(hdr));
            memcpy(tx_buf + sizeof(hdr), &alive, sizeof(alive));

            FreeRTOS_send(
                g_someip_clients[i].socket,
                tx_buf,
                sizeof(hdr) + sizeof(alive),
                0
            );
        }
    }
}
