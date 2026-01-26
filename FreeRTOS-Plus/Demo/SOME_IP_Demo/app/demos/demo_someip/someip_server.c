#include "someip_server.h"

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"
#include "someip_core/someip_server_state.h"
#include "app/demos/demo_someip/someip_core/someip_client_task.c"

#include <string.h>

/* =========================================================
 * Global client table (definition)
 * ========================================================= */
someip_client_ctx_t g_someip_clients[SOMEIP_MAX_CLIENTS];

/* =========================================================
 * Forward declarations (NO .c includes!)
 * ========================================================= */
extern void someip_client_task(void *arg);
extern void someip_notification_task(void *arg);

static void someip_server_task(void *arg);
static someip_client_ctx_t* allocate_client_slot(Socket_t sock);

/* =========================================================
 * Public entry
 * ========================================================= */
void someip_server_start(void)
{
    /* Initialize client table */
    memset(g_someip_clients, 0, sizeof(g_someip_clients));

    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        g_someip_clients[i].active = pdFALSE;
        g_someip_clients[i].client_state = CLIENT_DISCONNECTED;
        g_someip_clients[i].event_state = EVENT_NOT_SUBSCRIBED;
    }

    /* Start server task */
    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );

    /* Start notification timer task */
    xTaskCreate(
        someip_notification_task,
        "SOMEIP_NOTIFY",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
}

/* =========================================================
 * Helper: Allocate client slot
 * ========================================================= */
static someip_client_ctx_t* allocate_client_slot(Socket_t sock)
{
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (!g_someip_clients[i].active)
        {
            g_someip_clients[i].socket = sock;
            g_someip_clients[i].active = pdTRUE;
            g_someip_clients[i].client_state = CLIENT_DISCONNECTED;
            g_someip_clients[i].event_state = EVENT_NOT_SUBSCRIBED;
            g_someip_clients[i].last_activity_tick = xTaskGetTickCount();
            
            FreeRTOS_printf(("SOME/IP: Allocated client slot %d\r\n", i));
            return &g_someip_clients[i];
        }
    }
    
    FreeRTOS_printf(("SOME/IP: ERROR - No free client slots!\r\n"));
    return NULL;
}

/* =========================================================
 * SOME/IP TCP server task
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

        /* Allocate client context */
        someip_client_ctx_t *ctx = allocate_client_slot(client_sock);
        
        if (ctx == NULL)
        {
            FreeRTOS_printf(("SOME/IP: Rejecting client (no slots)\r\n"));
            FreeRTOS_closesocket(client_sock);
            continue;
        }

        /* Spawn RX task for this client */
        xTaskCreate(
            someip_client_task,
            "SOMEIP_CLIENT",
            configMINIMAL_STACK_SIZE * 4,
            (void *)client_sock,
            tskIDLE_PRIORITY + 1,
            &ctx->rx_task
        );
    }
}