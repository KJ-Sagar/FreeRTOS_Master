#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include "app/demos/demo_someip/someip_protocol.h"
#include "app/demos/demo_someip/someip_core/someip_server_state.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =========================================================
 * Constants
 * ========================================================= */
#define RX_TIMEOUT_MS        100
#define SOMEIP_HEADER_SIZE  (sizeof(someip_header_t))

/* =========================================================
 * Helper: Find client context by socket
 * ========================================================= */
static someip_client_ctx_t* find_client_by_socket(Socket_t sock)
{
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (g_someip_clients[i].active && g_someip_clients[i].socket == sock)
        {
            return &g_someip_clients[i];
        }
    }
    return NULL;
}

/* =========================================================
 * SOME/IP client RX task
 * ========================================================= */
void someip_client_task(void *arg)
{
    Socket_t client_sock = (Socket_t)arg;
    someip_client_ctx_t *ctx = find_client_by_socket(client_sock);

    if (ctx == NULL)
    {
        printf("SOME/IP: ERROR - No client context found!\r\n");
        /* Task exits naturally - no vTaskDelete needed if not enabled */
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

        /* ---------------------------------------------
         * No data → timeout (NORMAL, NOT DISCONNECT)
         * --------------------------------------------- */
        if (r == 0)
        {
            continue;
        }

        /* ---------------------------------------------
         * Real disconnect / socket error
         * --------------------------------------------- */
        if (r < 0)
        {
            printf("SOME/IP: Client disconnected (socket error)\r\n");
            break;
        }

        /* ---------------------------------------------
         * Partial header (ignore safely)
         * --------------------------------------------- */
        if (r != SOMEIP_HEADER_SIZE)
        {
            printf("SOME/IP: Partial header (%d bytes)\r\n", r);
            continue;
        }

        /* ---------------------------------------------
         * Decode SOME/IP header
         * --------------------------------------------- */
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

        /* Update state: valid message received */
        if (ctx->client_state == CLIENT_CONNECTED)
        {
            ctx->client_state = CLIENT_ACTIVE;
            printf("SOME/IP: State -> CLIENT_ACTIVE\r\n");
        }

        /* ---------------------------------------------
         * Drain payload if present
         * --------------------------------------------- */
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

        /* ---------------------------------------------
         * Handle subscription methods
         * --------------------------------------------- */
        if (hdr.method_id == SOMEIP_METHOD_SUBSCRIBE)
        {
            printf("SOME/IP: Client SUBSCRIBE received\r\n");
            ctx->event_state = EVENT_SUBSCRIBED;
            printf("SOME/IP: Event State -> EVENT_SUBSCRIBED\r\n");
        }
        else if (hdr.method_id == SOMEIP_METHOD_UNSUBSCRIBE)
        {
            printf("SOME/IP: Client UNSUBSCRIBE received\r\n");
            ctx->event_state = EVENT_NOT_SUBSCRIBED;
            printf("SOME/IP: Event State -> EVENT_NOT_SUBSCRIBED\r\n");
        }

        /* ---------------------------------------------
         * Send ACK
         * --------------------------------------------- */
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

    /* -------------------------------------------------
     * Cleanup on disconnect
     * ------------------------------------------------- */
    printf("SOME/IP: Client RX task exiting\r\n");

    ctx->client_state = CLIENT_DISCONNECTED;
    ctx->event_state = EVENT_NOT_SUBSCRIBED;
    ctx->active = pdFALSE;

    FreeRTOS_closesocket(client_sock);

    /* Only use vTaskDelete if enabled in FreeRTOSConfig.h */
#if (INCLUDE_vTaskDelete == 1)
    vTaskDelete(NULL);
#else
    /* If vTaskDelete not available, task suspends itself indefinitely */
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000));
#endif
}