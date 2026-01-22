#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include "someip_server_state.h"
#include "app/demos/demo_someip/someip_protocol.h"
#include "app/demos/demo_someip/heartbeat_service.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =========================================================
 * Receive exactly N bytes (called only after select)
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
        else
        {
            return pdFAIL;
        }
    }

    return pdPASS;
}

/* =========================================================
 * SOME/IP client RX + response task (Option B compliant)
 * ========================================================= */
void someip_client_task(void *arg)
{
    someip_client_ctx_t *client = (someip_client_ctx_t *)arg;
    Socket_t sock = client->socket;

    uint8_t rx_buf[256];
    uint8_t tx_buf[64];

    printf("SOME/IP: Client handler started\r\n");

    for (;;)
    {
        /* -------------------------------------------------
         * Create socket set (FreeRTOS requires this)
         * ------------------------------------------------- */
        SocketSet_t rxSet = FreeRTOS_CreateSocketSet();
        configASSERT(rxSet != NULL);

        FreeRTOS_FD_SET(
            sock,
            rxSet,
            eSELECT_READ
        );

        BaseType_t ready =
            FreeRTOS_select(rxSet, pdMS_TO_TICKS(1000));

        /* -------------------------------------------------
         * Socket readable
         * ------------------------------------------------- */
        if (ready > 0 && FreeRTOS_FD_ISSET(sock, rxSet))
        {
<<<<<<< HEAD
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
=======
            int r = FreeRTOS_recv(
                sock,
                rx_buf,
>>>>>>> main
                sizeof(someip_header_t),
                0
            );

            if (r == sizeof(someip_header_t))
            {
                someip_header_t hdr;
                memcpy(&hdr, rx_buf, sizeof(hdr));
                someip_ntoh_header(&hdr);

                printf("SOME/IP HEADER:\r\n");
                printf("  Service ID : 0x%04x\r\n", hdr.service_id);
                printf("  Method ID  : 0x%04x\r\n", hdr.method_id);
                printf("  Client ID  : 0x%04x\r\n", hdr.client_id);
                printf("  Session ID : 0x%04x\r\n", hdr.session_id);
                printf("  Length     : %lu\r\n",
                       (unsigned long)hdr.length);
                printf("  Msg Type   : 0x%02x\r\n", hdr.message_type);
                printf("  Ret Code   : 0x%02x\r\n", hdr.return_code);

                /* Subscribe / Unsubscribe */
                if (hdr.method_id == SOMEIP_METHOD_SUBSCRIBE)
                {
                    client->heartbeat_subscribed = pdTRUE;
                    printf("SOME/IP: Client subscribed to heartbeat\r\n");
                }
                else if (hdr.method_id == SOMEIP_METHOD_UNSUBSCRIBE)
                {
                    client->heartbeat_subscribed = pdFALSE;
                    printf("SOME/IP: Client unsubscribed from heartbeat\r\n");
                }

                /* Receive payload if present */
                uint32_t payload_len =
                    hdr.length - SOMEIP_HEADER_PAYLOAD_OFFSET;

                if (payload_len > 0)
                {
                    if (recv_exact(sock, rx_buf, payload_len) != pdPASS)
                    {
                        printf("SOME/IP: Payload receive failed\r\n");
                        FreeRTOS_DeleteSocketSet(rxSet);
                        break;
                    }
                }

                /* Build ACK response */
                hdr.message_type = SOMEIP_MSG_RESPONSE;
                hdr.return_code  = SOMEIP_RET_OK;
                hdr.length       = SOMEIP_HEADER_PAYLOAD_OFFSET;
                hdr.client_id    = 0x0000;

                someip_hton_header(&hdr);
                memcpy(tx_buf, &hdr, sizeof(hdr));

                FreeRTOS_send(
                    sock,
                    tx_buf,
                    sizeof(someip_header_t),
                    0
                );

                printf("SOME/IP: Sent ACK response\r\n");
            }
            else
            {
                printf("SOME/IP: Client disconnected\r\n");
                FreeRTOS_DeleteSocketSet(rxSet);
                break;
            }
        }

        FreeRTOS_DeleteSocketSet(rxSet);
    }

    /* -------------------------------------------------
     * Cleanup
     * ------------------------------------------------- */
    client->active = pdFALSE;
    client->heartbeat_subscribed = pdFALSE;

    FreeRTOS_closesocket(sock);

    printf("SOME/IP: Client task stopped\r\n");

    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
}
