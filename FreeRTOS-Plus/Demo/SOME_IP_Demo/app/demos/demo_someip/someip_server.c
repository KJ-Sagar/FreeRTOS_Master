#include "someip_server.h"

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"
#include "someip_core/someip_client_task.c"
#include <string.h>
#include "someip_core/someip_server_state.h"

someip_client_ctx_t g_someip_clients[SOMEIP_MAX_CLIENTS];

extern void someip_client_task(void *arg);

/* =========================================================
 * Configuration
 * ========================================================= */
#define SOMEIP_MAX_CLIENTS  4

/* =========================================================
 * Forward declaration
 * ========================================================= */
static void someip_server_task(void *arg);

/* =========================================================
 * Public entry
 * ========================================================= */
void someip_server_start(void)
{
    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );
}

/* =========================================================
 * SOME/IP TCP server task
 *
 * Phase 2B (Step 1):
 *  - Only accepts connections
 *  - Does NOT process client data
 * ========================================================= */
static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t listen_sock;
    Socket_t client_sock;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    listen_sock = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    configASSERT(listen_sock != FREERTOS_INVALID_SOCKET);

    memset(&addr, 0, sizeof(addr));
    addr.sin_port = FreeRTOS_htons(SOMEIP_SERVER_PORT);

    FreeRTOS_bind(listen_sock, &addr, sizeof(addr));
    FreeRTOS_listen(listen_sock, SOMEIP_MAX_CLIENTS);

    FreeRTOS_printf(("SOME/IP: Listening on %u\r\n", SOMEIP_SERVER_PORT));

    for (;;)
    {
        client_sock = FreeRTOS_accept(listen_sock, &addr, &addrlen);
        if (client_sock == FREERTOS_INVALID_SOCKET)
            continue;

        FreeRTOS_printf(("SOME/IP: Client connected\r\n"));

xTaskCreate(
    someip_client_task,
    "SOMEIP_CLIENT",
    configMINIMAL_STACK_SIZE * 4,
    (void *)client_sock,
    tskIDLE_PRIORITY + 1,
    NULL
);

    }
}
