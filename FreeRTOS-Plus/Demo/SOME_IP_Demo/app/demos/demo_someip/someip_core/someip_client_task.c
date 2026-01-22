#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "app/demos/demo_someip/someip_protocol.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ========================================================= */
#define SERVICE_HEARTBEAT   0x1234
#define METHOD_HEARTBEAT    0x0001
#define METHOD_SUBSCRIBE    0x0100
#define METHOD_UNSUBSCRIBE  0x0101

#define HEARTBEAT_PERIOD_MS 2000
#define RX_TIMEOUT_MS       100

/* ========================================================= */
static BaseType_t recv_exact(Socket_t sock, uint8_t *buf, size_t len)
{
    size_t received = 0;

    while (received < len)
    {
        int r = FreeRTOS_recv(sock, buf + received, len - received, 0);
        if (r > 0)
            received += (size_t)r;
        else
            return pdFAIL;
    }
    return pdPASS;
}

/* ========================================================= */
void someip_client_task(void *arg)
{
    Socket_t sock = (Socket_t)arg;

    uint8_t rx_buf[256];
    uint8_t tx_buf[64];

    BaseType_t heartbeat_subscribed = pdFALSE;
    TickType_t last_hb_tick = 0;

    TickType_t timeout = pdMS_TO_TICKS(RX_TIMEOUT_MS);
    FreeRTOS_setsockopt(
        sock,
        0,
        FREERTOS_SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );

    printf("SOME/IP: Client handler started\r\n");

    for (;;)
    {
        /* ---------- RX EVENT ---------- */
        int r = FreeRTOS_recv(sock, rx_buf, sizeof(someip_header_t), 0);

        if (r == sizeof(someip_header_t))
        {
            someip_header_t hdr;
            memcpy(&hdr, rx_buf, sizeof(hdr));
            someip_ntoh_header(&hdr);

            printf("SOME/IP RX: SID=0x%04x MID=0x%04x\r\n",
                   hdr.service_id, hdr.method_id);

            if (hdr.method_id == METHOD_SUBSCRIBE)
            {
                heartbeat_subscribed = pdTRUE;
                printf("SOME/IP: Subscribed\r\n");
            }
            else if (hdr.method_id == METHOD_UNSUBSCRIBE)
            {
                heartbeat_subscribed = pdFALSE;
                printf("SOME/IP: Unsubscribed\r\n");
            }

            uint32_t payload_len = hdr.length - 8;
            if (payload_len > 0)
            {
                if (recv_exact(sock, rx_buf, payload_len) != pdPASS)
                    break;
            }

            hdr.message_type = SOMEIP_MSG_RESPONSE;
            hdr.return_code  = SOMEIP_RET_OK;
            hdr.length       = 8;
            hdr.client_id    = 0;

            someip_hton_header(&hdr);
            memcpy(tx_buf, &hdr, sizeof(hdr));
            FreeRTOS_send(sock, tx_buf, sizeof(hdr), 0);
        }
        else if (r < 0)
        {
            printf("SOME/IP: Client disconnected\r\n");
            break;
        }

        /* ---------- PERIODIC HEARTBEAT ---------- */
        if (heartbeat_subscribed)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_hb_tick) >= pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS))
            {
                last_hb_tick = now;

                someip_header_t nhdr;
                uint32_t alive = FreeRTOS_htonl(1);

                nhdr.service_id        = SERVICE_HEARTBEAT;
                nhdr.method_id         = METHOD_HEARTBEAT;
                nhdr.client_id         = 0;
                nhdr.session_id        = 0;
                nhdr.protocol_version  = SOMEIP_PROTOCOL_VERSION;
                nhdr.interface_version = SOMEIP_INTERFACE_VERSION;
                nhdr.message_type      = SOMEIP_MSG_NOTIFICATION;
                nhdr.return_code       = SOMEIP_RET_OK;
                nhdr.length            = 12;

                someip_hton_header(&nhdr);
                memcpy(tx_buf, &nhdr, sizeof(nhdr));
                memcpy(tx_buf + sizeof(nhdr), &alive, sizeof(alive));

                FreeRTOS_send(
                    sock,
                    tx_buf,
                    sizeof(nhdr) + sizeof(alive),
                    0
                );

                printf("SOME/IP: Heartbeat sent\r\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    FreeRTOS_closesocket(sock);
    printf("SOME/IP: Client task stopped\r\n");

    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
}
