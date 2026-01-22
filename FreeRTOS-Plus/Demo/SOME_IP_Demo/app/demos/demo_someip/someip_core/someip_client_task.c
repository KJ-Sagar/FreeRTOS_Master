#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "app/demos/demo_someip/someip_protocol.h"
#include "app/demos/demo_someip/heartbeat_service.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =========================================================
 * Receive exactly N bytes from TCP
 * ========================================================= */
static BaseType_t recv_exact(Socket_t sock, uint8_t *buf, size_t len)
{
    size_t received = 0;

    while (received < len)
    {
        int r = FreeRTOS_recv(sock, buf + received, len - received, 0);

        if (r > 0)
        {
            received += (size_t)r;
        }
        else if (r == 0)
        {
            /* timeout */
            return pdFAIL;
        }
        else
        {
            return pdFAIL;
        }
    }

    return pdPASS;
}

/* =========================================================
 * Client handler task
 * ========================================================= */
void someip_client_task(void *arg)
{
    Socket_t client_sock = (Socket_t)arg;

    uint8_t rx_buf[256];
    uint8_t tx_buf[64];

    BaseType_t heartbeat_subscribed = pdFALSE;
    TickType_t last_notify = 0;

    /* Receive timeout */
    TickType_t timeout = pdMS_TO_TICKS(100);
    FreeRTOS_setsockopt(
        client_sock,
        0,
        FREERTOS_SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );

    printf("SOME/IP: Client handler started\r\n");

    for (;;)
    {
        /* =================================================
         * 1. Handle incoming request (if any)
         * ================================================= */
        if (recv_exact(client_sock, rx_buf, sizeof(someip_header_t)) == pdPASS)
        {
            someip_header_t hdr;

            memcpy(&hdr, rx_buf, sizeof(hdr));
            someip_ntoh_header(&hdr);

            printf("SOME/IP HEADER:\r\n");
            printf("  Service ID : 0x%04x\r\n", hdr.service_id);
            printf("  Method ID  : 0x%04x\r\n", hdr.method_id);
            printf("  Client ID  : 0x%04x\r\n", hdr.client_id);
            printf("  Session ID : 0x%04x\r\n", hdr.session_id);
            printf("  Length     : %lu\r\n", hdr.length);
            printf("  Msg Type   : 0x%02x\r\n", hdr.message_type);
            printf("  Ret Code   : 0x%02x\r\n", hdr.return_code);

            if (hdr.method_id == SOMEIP_METHOD_SUBSCRIBE)
            {
                heartbeat_subscribed = pdTRUE;
                printf("SOME/IP: Client subscribed to heartbeat\r\n");
            }

            uint32_t payload_len = hdr.length - 8;

            /* Receive payload if present */
            if (payload_len > 0)
            {
                if (recv_exact(client_sock, rx_buf, payload_len) == pdFAIL)
                {
                    printf("SOME/IP: Payload receive failed\r\n");
                    FreeRTOS_closesocket(client_sock);
                    break;
                }
            }

            /* Build ACK response */
            hdr.message_type = SOMEIP_MSG_RESPONSE;
            hdr.return_code  = SOMEIP_RET_OK;
            hdr.length       = 8;
            hdr.client_id    = 0x0000; /* server */

            someip_hton_header(&hdr);
            memcpy(tx_buf, &hdr, sizeof(hdr));

            FreeRTOS_send(
                client_sock,
                tx_buf,
                sizeof(someip_header_t),
                0
            );

            printf("SOME/IP: Sent ACK response\r\n");
        }

        /* =================================================
         * 2. Periodic heartbeat notification
         * ================================================= */
        if (heartbeat_subscribed)
        {
            TickType_t now = xTaskGetTickCount();

            if ((now - last_notify) > pdMS_TO_TICKS(2000))
            {
                last_notify = now;

                someip_header_t nhdr;
                uint32_t alive = FreeRTOS_htonl(1);

                nhdr.service_id        = SERVICE_HEARTBEAT;
                nhdr.method_id         = METHOD_HEARTBEAT;
                nhdr.client_id         = 0x0000;
                nhdr.session_id        = 0x0000;
                nhdr.protocol_version  = SOMEIP_PROTOCOL_VERSION;
                nhdr.interface_version = SOMEIP_INTERFACE_VERSION;
                nhdr.message_type      = SOMEIP_MSG_NOTIFICATION;
                nhdr.return_code       = SOMEIP_RET_OK;
                nhdr.length            = 12; /* 8 + 4 */

                someip_hton_header(&nhdr);
                memcpy(tx_buf, &nhdr, sizeof(nhdr));
                memcpy(tx_buf + sizeof(nhdr), &alive, sizeof(alive));

                FreeRTOS_send(
                    client_sock,
                    tx_buf,
                    sizeof(nhdr) + sizeof(alive),
                    0
                );

                printf("SOME/IP: Heartbeat notification sent\r\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    printf("SOME/IP: Client disconnected\r\n");
    FreeRTOS_closesocket(client_sock);

    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
}
