#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include "someip_protocol.h"
#include "someip_core/someip_server_state.h"
#include "someip_eventgroup.h"

#include <stdio.h>
#include <string.h>

/* =========================================================
 * Configuration
 * ========================================================= */
#define NOTIFICATION_PERIOD_MS  2000  /* 2 seconds */

/* Heartbeat service definition */
#define SERVICE_HEARTBEAT  0x1234
#define EVENT_HEARTBEAT    0x0001
#define EVENTGROUP_STATUS  0x0001

/* =========================================================
 * Heartbeat counter (simulated sensor data)
 * ========================================================= */
static uint32_t heartbeat_counter = 0;

/* =========================================================
 * Helper: Build and send notification to ONE client
 * ========================================================= */
static BaseType_t send_heartbeat_notification(someip_client_ctx_t *ctx)
{
    someip_header_t hdr;
    uint8_t tx_buf[sizeof(someip_header_t) + 4];
    uint32_t payload;

    /* Build header */
    hdr.service_id = SERVICE_HEARTBEAT;
    hdr.method_id = EVENT_HEARTBEAT;
    hdr.length = SOMEIP_LENGTH_FIELD(4);  /* 8 + 4 bytes payload */
    hdr.client_id = 0x0000;
    hdr.session_id = 0x0000;
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
        ctx->notifications_sent++;
        return pdTRUE;
    }

    return pdFALSE;
}

/* =========================================================
 * Notification broadcaster task
 * 
 * Phase 4 Enhancement:
 *  - Broadcasts to ALL active clients
 *  - Checks subscription state per client
 *  - Isolates failures (one client error doesn't affect others)
 *  - Respects event group membership
 * ========================================================= */
void someip_notification_task(void *arg)
{
    (void)arg;

    printf("SOME/IP: Notification task started\r\n");

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(NOTIFICATION_PERIOD_MS));

        /* Broadcast to all active clients */
        uint8_t sent_count = 0;
        uint8_t subscriber_count = 0;

        for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
        {
            someip_client_ctx_t *ctx = &g_someip_clients[i];

            /* Skip inactive clients */
            if (!ctx->active)
                continue;

            /* Skip if not in correct state */
            if (ctx->client_state != CLIENT_ACTIVE)
                continue;

            /* Check if subscribed to heartbeat event group */
            if (someip_client_is_subscribed(ctx, SERVICE_HEARTBEAT, EVENTGROUP_STATUS))
            {
                subscriber_count++;

                /* Send notification */
                if (send_heartbeat_notification(ctx) == pdTRUE)
                {
                    sent_count++;
                }
                else
                {
                    printf("SOME/IP: Failed to send notification to client[%d]\r\n", i);
                }
            }
        }

        /* Log broadcast summary (only if there were subscribers) */
        if (subscriber_count > 0)
        {
            printf("SOME/IP: Broadcast notification %lu to %u client(s) (%u sent)\r\n",
                   (unsigned long)heartbeat_counter, 
                   subscriber_count, 
                   sent_count);
        }

        /* Increment counter for next round */
        heartbeat_counter++;
    }
}