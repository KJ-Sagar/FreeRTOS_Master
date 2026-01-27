#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include "app/demos/demo_someip/someip_protocol.h"
#include "app/demos/demo_someip/someip_core/someip_server_state.h"
#include "app/demos/demo_someip/someip_core/someip_eventgroup.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =========================================================
 * Constants
 * ========================================================= */
#define RX_TIMEOUT_MS        100
#define SOMEIP_HEADER_SIZE  (sizeof(someip_header_t))

/* Default event group if not specified in payload */
#define DEFAULT_EVENTGROUP_ID  0x0001

/* =========================================================
 * SOME/IP client RX task
 * ========================================================= */
void someip_client_task(void *arg)
{
    Socket_t client_sock = (Socket_t)arg;
    someip_client_ctx_t *ctx = someip_client_find_by_socket(client_sock);

    if (ctx == NULL)
    {
        printf("SOME/IP: ERROR - No client context found!\r\n");
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    uint8_t rx_buf[256];
    uint8_t tx_buf[SOMEIP_HEADER_SIZE];

    TickType_t timeout = pdMS_TO_TICKS(RX_TIMEOUT_MS);

    /* Set receive timeout */
    FreeRTOS_setsockopt(
        client_sock,
        0,
        FREERTOS_SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );

    printf("SOME/IP: Client RX task started\r\n");

    /* Update state: socket connected */
    ctx->client_state = CLIENT_CONNECTED;
    printf("SOME/IP: State -> CLIENT_CONNECTED\r\n");

    for (;;)
    {
        int r = FreeRTOS_recv(
            client_sock,
            rx_buf,
            SOMEIP_HEADER_SIZE,
            0
        );

        if (r == 0)
        {
            continue;  /* Timeout */
        }

        if (r < 0)
        {
            printf("SOME/IP: Client disconnected (socket error)\r\n");
            break;
        }

        if (r != SOMEIP_HEADER_SIZE)
        {
            printf("SOME/IP: Partial header (%d bytes)\r\n", r);
            continue;
        }

        /* Decode SOME/IP header */
        someip_header_t hdr;
        memcpy(&hdr, rx_buf, SOMEIP_HEADER_SIZE);
        someip_ntoh_header(&hdr);

        printf("SOME/IP RX:\r\n");
        printf("  Service ID : 0x%04x\r\n", hdr.service_id);
        printf("  Method ID  : 0x%04x\r\n", hdr.method_id);
        printf("  Client ID  : 0x%04x\r\n", hdr.client_id);
        printf("  Session ID : 0x%04x\r\n", hdr.session_id);
        printf("  Length     : %lu\r\n", (unsigned long)hdr.length);
        printf("  Msg Type   : 0x%02x\r\n", hdr.message_type);
        printf("  Ret Code   : 0x%02x\r\n", hdr.return_code);

        /* Update statistics */
        ctx->messages_received++;
        ctx->last_activity_tick = xTaskGetTickCount();

        /* Update state: valid message received */
        if (ctx->client_state == CLIENT_CONNECTED)
        {
            ctx->client_state = CLIENT_ACTIVE;
            printf("SOME/IP: State -> CLIENT_ACTIVE\r\n");
        }

        /* Drain payload if present */
        uint32_t payload_len = 0;
        if (hdr.length > SOMEIP_HEADER_PAYLOAD_OFFSET)
        {
            payload_len = hdr.length - SOMEIP_HEADER_PAYLOAD_OFFSET;
        }

        if (payload_len > 0)
        {
            if (payload_len > sizeof(rx_buf))
            {
                printf("SOME/IP: Payload too large\r\n");
                break;
            }

            int pr = FreeRTOS_recv(
                client_sock,
                rx_buf,
                payload_len,
                0
            );

            if (pr <= 0)
            {
                printf("SOME/IP: Client disconnected (payload)\r\n");
                break;
            }
        }

        /* Parse event group ID from payload (if present) */
        uint16_t eventgroup_id = DEFAULT_EVENTGROUP_ID;
        uint32_t ttl_seconds = 5;  /* Default TTL */

        if (payload_len >= 2)
        {
            /* First 2 bytes of payload = eventgroup_id (network byte order) */
            eventgroup_id = (rx_buf[0] << 8) | rx_buf[1];
        }

        if (payload_len >= 6)
        {
            /* Next 4 bytes = TTL (network byte order) */
            ttl_seconds = (rx_buf[2] << 24) | (rx_buf[3] << 16) |
                         (rx_buf[4] << 8) | rx_buf[5];
        }

        /* Handle subscription methods */
        if (hdr.method_id == SOMEIP_METHOD_SUBSCRIBE)
        {
            printf("SOME/IP: Client SUBSCRIBE received\r\n");
            printf("  Event Group: 0x%04x\r\n", eventgroup_id);
            printf("  TTL: %lu seconds\r\n", (unsigned long)ttl_seconds);

            BaseType_t result = someip_client_subscribe(
                ctx,
                hdr.service_id,
                eventgroup_id,
                ttl_seconds
            );

            if (result == pdPASS)
            {
                printf("SOME/IP: Subscription successful\r\n");
            }
            else
            {
                printf("SOME/IP: Subscription failed\r\n");
            }
        }
        else if (hdr.method_id == SOMEIP_METHOD_UNSUBSCRIBE)
        {
            printf("SOME/IP: Client UNSUBSCRIBE received\r\n");
            printf("  Event Group: 0x%04x\r\n", eventgroup_id);

            BaseType_t result = someip_client_unsubscribe(
                ctx,
                hdr.service_id,
                eventgroup_id
            );

            if (result == pdPASS)
            {
                printf("SOME/IP: Unsubscription successful\r\n");
            }
            else
            {
                printf("SOME/IP: Unsubscription failed\r\n");
            }
        }

        /* Send ACK */
        hdr.message_type = SOMEIP_MSG_RESPONSE;
        hdr.return_code  = SOMEIP_RET_OK;
        hdr.length       = SOMEIP_HEADER_PAYLOAD_OFFSET;
        hdr.client_id    = 0x0000;

        someip_hton_header(&hdr);
        memcpy(tx_buf, &hdr, SOMEIP_HEADER_SIZE);

        FreeRTOS_send(
            client_sock,
            tx_buf,
            SOMEIP_HEADER_SIZE,
            0
        );

        printf("SOME/IP: ACK sent\r\n");
    }

    /* Cleanup on disconnect */
    printf("SOME/IP: Client RX task exiting\r\n");

    someip_client_free(ctx);
    FreeRTOS_closesocket(client_sock);

#if (INCLUDE_vTaskDelete == 1)
    vTaskDelete(NULL);
#else
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000));
#endif
}