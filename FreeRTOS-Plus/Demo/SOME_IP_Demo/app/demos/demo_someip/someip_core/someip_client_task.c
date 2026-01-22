#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "app/demos/demo_someip/someip_protocol.h"

#include <stdio.h>
#include <stdint.h>

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
            /* timeout – keep waiting */
            continue;
        }
        else
        {
            /* real disconnect */
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
    uint8_t buf[256];
    uint8_t tx_buf[64]; /* For future use */
    BaseType_t heartbeat_subscribed = pdFALSE;

    /* Set receive timeout */
    TickType_t timeout = pdMS_TO_TICKS(1000);
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
 if (recv_exact(client_sock, buf, 16) == pdPASS)
{
    someip_header_t hdr;

    memcpy(&hdr, buf, sizeof(hdr));
    someip_ntoh_header(&hdr);

    printf("SOME/IP HEADER:\r\n");
    printf("  Service ID : 0x%04x\r\n", hdr.service_id);
    printf("  Method ID  : 0x%04x\r\n", hdr.method_id);
    printf("  Client ID  : 0x%04x\r\n", hdr.client_id);
    printf("  Session ID : 0x%04x\r\n", hdr.session_id);
    printf("  Length     : %u\r\n", hdr.length);
    printf("  Msg Type   : 0x%02x\r\n", hdr.message_type);
    printf("  Ret Code   : 0x%02x\r\n", hdr.return_code);

    if (hdr.method_id == SOMEIP_METHOD_SUBSCRIBE)
{
    heartbeat_subscribed = pdTRUE;
    printf("SOME/IP: Client subscribed to heartbeat\r\n");
}

    uint32_t payload_len = hdr.length - 8;

    /* Build minimal SOME/IP response (ACK) */
hdr.message_type = SOMEIP_MSG_RESPONSE;
hdr.return_code  = SOMEIP_RET_OK;
hdr.length       = 8;  /* header only */

/* Swap client/server IDs (required) */
uint16_t tmp = hdr.client_id;
hdr.client_id  = 0x0000;   /* server */
hdr.session_id = hdr.session_id;

/* Send response */
someip_hton_header(&hdr);
memcpy(tx_buf, &hdr, sizeof(hdr));

FreeRTOS_send(
    client_sock,
    tx_buf,
    sizeof(someip_header_t),
    0
);

printf("SOME/IP: Sent ACK response\r\n");
    /* Receive payload if any */
    if (payload_len > 0)
    {
        if (recv_exact(client_sock, buf, payload_len) == pdPASS)
        {
            printf("SOME/IP: Received payload (%u bytes)\r\n", payload_len);
        }
        else
        {
            printf("SOME/IP: Payload receive failed\r\n");
            FreeRTOS_closesocket(client_sock);
            for (;;)
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
else
{
    printf("SOME/IP: Client disconnected\r\n");
    FreeRTOS_closesocket(client_sock);
    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
}
    }
}
