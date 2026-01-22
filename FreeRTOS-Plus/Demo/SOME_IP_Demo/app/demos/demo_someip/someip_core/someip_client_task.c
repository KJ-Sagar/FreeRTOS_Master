#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#include <stdio.h>

/* =========================================================
 * Helper: Receive exact number of bytes
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
            continue;
        }
        else
        {
            return pdFAIL;
        }
    }

    return pdPASS;
}


/* =========================================================
 * Client handler task (EMPTY for now)
 * ========================================================= */
void someip_client_task(void *arg)
{
    Socket_t client_sock = (Socket_t)arg;
    uint8_t buf[64];

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
        int r = FreeRTOS_recv(client_sock, buf, sizeof(buf), 0);

        if (r > 0)
        {
            printf("SOME/IP: Received %d bytes from client\r\n", r);
        }
        else if (r == 0)
        {
            /* timeout – do nothing, keep connection */
            continue;
        }
        else
        {
            /* real disconnect */
            printf("SOME/IP: Client disconnected\r\n");
            FreeRTOS_closesocket(client_sock);

            for (;;)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}
