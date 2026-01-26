#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include "app/demos/demo_someip/someip_protocol.h"
#include "app/demos/demo_someip/someip_core/someip_server_state.h"

#include <stdio.h>
#include <string.h>

/* =========================================================
 * Notification period (ITCG_0013 requirement)
 * ========================================================= */
#define NOTIFICATION_PERIOD_MS  2000  // 2 seconds

/* =========================================================
 * Heartbeat counter (simulated sensor data)
 * ========================================================= */
static uint32_t heartbeat_counter = 0;

/* =========================================================
 * Helper: Build and send notification
 * ========================================================= */
static void send_heartbeat_notification(someip_client_ctx_t *ctx)
{
    someip_header_t hdr;
    uint8_t tx_buf[sizeof(someip_header_t) + 4];  // Header + uint32 payload
    uint32_t payload;

    /* Build header */
    hdr.service_id = 0x1234;  // SERVICE_HEARTBEAT from Python client
    hdr.method_id = 0x0001;   // METHOD_HEARTBEAT
    hdr.length = SOMEIP_LENGTH_FIELD(4);  // 8 + 4 bytes payload
    hdr.client_id = 0x0000;
    hdr.session_id = 0x0000;  // Notifications don't track sessions
    hdr.protocol_version = SOMEIP_PROTOCOL_VERSION;
    hdr.interface_version = SOMEIP_INTERFACE_VERSION;
    hdr.message_type = SOMEIP_MSG_NOTIFICATION;
    hdr.return_code = 0x00;

    /* Convert to network byte order */
    someip_hton_header(&hdr);

    /* Build payload (heartbeat counter) */
    payload = FreeRTOS_htonl(heartbeat_counter);

    /* Copy to transmit buffer */
    memcpy(tx_buf, &hdr, sizeof(someip_header_t));
    memcpy(tx_buf + sizeof(someip_header_t), &payload, 4);

    /* Send notification */
    int sent = FreeRTOS_send(
        ctx->socket,
        tx_buf,
        sizeof(tx_buf),
        0
    );

    if (sent > 0)
    {
        printf("SOME/IP: NOTIFICATION sent (counter=%lu)\r\n", 
               (unsigned long)heartbeat_counter);
        heartbeat_counter++;
    }
    else
    {
        printf("SOME/IP: NOTIFICATION send failed\r\n");
    }
}

/* =========================================================
 * Notification timer task
 * 
 * ITCG_0013 Requirement:
 *  - Send notifications ONLY if client is subscribed
 *  - Stop immediately on unsubscribe
 * ========================================================= */
void someip_notification_task(void *arg)
{
    printf("SOME/IP: Notification task started\r\n");

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(NOTIFICATION_PERIOD_MS));

        /* Check all clients for subscriptions */
        for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
        {
            someip_client_ctx_t *ctx = &g_someip_clients[i];

            /* Skip inactive clients */
            if (!ctx->active)
                continue;

            /* Skip if not in correct state */
            if (ctx->client_state != CLIENT_ACTIVE)
                continue;

            /* CRITICAL: Only send if subscribed */
            if (ctx->event_state == EVENT_SUBSCRIBED)
            {
                printf("SOME/IP: Client[%d] is subscribed, sending notification\r\n", i);
                send_heartbeat_notification(ctx);
            }
            else
            {
                /* Optional: Log when skipping (remove in production) */
                // printf("SOME/IP: Client[%d] NOT subscribed, skipping notification\r\n", i);
            }
        }
    }
}