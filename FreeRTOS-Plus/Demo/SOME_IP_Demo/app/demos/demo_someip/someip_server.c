#include "someip_server.h"
#include "someip_protocol.h"
#include "someip_core/someip_core.h"
#include "someip_core/someip_validate.h"

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#include <string.h>

/* =========================================================
 * Configuration
 * ========================================================= */
#define SOMEIP_MAX_CLIENTS        4
#define SOMEIP_RECV_TIMEOUT_MS    5000
#define SOMEIP_NOTIFY_PERIOD_MS   2000

/* =========================================================
 * Client context
 * ========================================================= */
typedef struct
{
    Socket_t socket;
    BaseType_t active;
    BaseType_t heartbeat_subscribed;
} someip_client_ctx_t;

static someip_client_ctx_t clients[SOMEIP_MAX_CLIENTS];

/* =========================================================
 * Forward declarations
 * ========================================================= */
static void someip_server_task(void *arg);
static void someip_notification_task(void *arg);

/* =========================================================
 * Helpers
 * ========================================================= */
static someip_client_ctx_t *alloc_client(Socket_t sock)
{
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (!clients[i].active)
        {
            clients[i].active = pdTRUE;
            clients[i].socket = sock;
            clients[i].heartbeat_subscribed = pdFALSE;
            return &clients[i];
        }
    }
    return NULL;
}

static void free_client(Socket_t sock)
{
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (clients[i].active && clients[i].socket == sock)
        {
            clients[i].active = pdFALSE;
            clients[i].heartbeat_subscribed = pdFALSE;
            clients[i].socket = FREERTOS_INVALID_SOCKET;
        }
    }
}

static BaseType_t recv_exact(Socket_t sock, uint8_t *buf, size_t len)
{
    size_t received = 0;

    while (received < len)
    {
        int r = FreeRTOS_recv(sock, buf + received, len - received, 0);
        if (r <= 0)
            return pdFAIL;
        received += (size_t)r;
    }
    return pdPASS;
}

/* =========================================================
 * Public entry
 * ========================================================= */
void someip_server_start(void)
{
    memset(clients, 0, sizeof(clients));

    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );

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
 * SOME/IP Server Task (multi-client)
 * ========================================================= */
static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t listen_sock, client_sock;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    uint8_t rx_buf[SOMEIP_RX_BUFFER_SIZE];
    uint8_t tx_buf[SOMEIP_TX_BUFFER_SIZE];

    listen_sock = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    configASSERT(listen_sock != FREERTOS_INVALID_SOCKET);

    addr.sin_port = FreeRTOS_htons(SOMEIP_SERVER_PORT);
    FreeRTOS_bind(listen_sock, &addr, sizeof(addr));
    FreeRTOS_listen(listen_sock, SOMEIP_MAX_CLIENTS);

    FreeRTOS_printf(("SOME/IP: Listening on %u\r\n", SOMEIP_SERVER_PORT));

    for (;;)
    {
        client_sock = FreeRTOS_accept(listen_sock, &addr, &addrlen);
        if (client_sock == FREERTOS_INVALID_SOCKET)
            continue;

        someip_client_ctx_t *client = alloc_client(client_sock);
        if (!client)
        {
            FreeRTOS_closesocket(client_sock);
            continue;
        }

        TickType_t timeout = pdMS_TO_TICKS(SOMEIP_RECV_TIMEOUT_MS);
        FreeRTOS_setsockopt(
            client_sock,
            0,
            FREERTOS_SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        );

        FreeRTOS_printf(("SOME/IP: Client connected\r\n"));

        for (;;)
        {
            someip_header_t hdr;

            if (recv_exact(client_sock, rx_buf, sizeof(hdr)) != pdPASS)
                break;

            memcpy(&hdr, rx_buf, sizeof(hdr));
            someip_ntoh_header(&hdr);

            if (someip_validate_header(&hdr) != pdPASS)
                break;

            uint32_t payload_len =
                hdr.length - SOMEIP_HEADER_PAYLOAD_OFFSET;

            if (payload_len >
                (SOMEIP_RX_BUFFER_SIZE - sizeof(hdr)))
                break;

            if (payload_len > 0 &&
                recv_exact(client_sock,
                           rx_buf + sizeof(hdr),
                           payload_len) != pdPASS)
                break;

            /* ---- Subscription handling ---- */
            if (hdr.method_id == 0x0100) /* SUBSCRIBE */
            {
                client->heartbeat_subscribed = pdTRUE;
                hdr.length = SOMEIP_HEADER_PAYLOAD_OFFSET;
                hdr.message_type = SOMEIP_MSG_RESPONSE;
                hdr.return_code = SOMEIP_RET_OK;
                goto send;
            }
            else if (hdr.method_id == 0x0101) /* UNSUBSCRIBE */
            {
                client->heartbeat_subscribed = pdFALSE;
                hdr.length = SOMEIP_HEADER_PAYLOAD_OFFSET;
                hdr.message_type = SOMEIP_MSG_RESPONSE;
                hdr.return_code = SOMEIP_RET_OK;
                goto send;
            }

            /* ---- Normal dispatch ---- */
            someip_service_handler_t handler =
                someip_find_service(hdr.service_id);

            uint32_t resp_len = 0;
            someip_return_code_t ret = SOMEIP_RET_OK;

            if (!handler ||
                handler(hdr.service_id,
                        hdr.method_id,
                        rx_buf + sizeof(hdr),
                        payload_len,
                        tx_buf + sizeof(hdr),
                        &resp_len,
                        &ret) != pdPASS)
            {
                ret = SOMEIP_RET_E_UNKNOWN_METHOD;
            }

            hdr.message_type =
                (ret == SOMEIP_RET_OK)
                    ? SOMEIP_MSG_RESPONSE
                    : SOMEIP_MSG_ERROR;

            hdr.return_code = ret;
            hdr.length = SOMEIP_HEADER_PAYLOAD_OFFSET + resp_len;

        send:
            someip_hton_header(&hdr);
            memcpy(tx_buf, &hdr, sizeof(hdr));
            FreeRTOS_send(
                client_sock,
                tx_buf,
                sizeof(hdr) + resp_len,
                0
            );
        }

        FreeRTOS_printf(("SOME/IP: Client disconnected\r\n"));
        free_client(client_sock);
        FreeRTOS_shutdown(client_sock, FREERTOS_SHUT_RDWR);
        FreeRTOS_closesocket(client_sock);
    }
}

/* =========================================================
 * Notification Task
 * ========================================================= */
static void someip_notification_task(void *arg)
{
    (void)arg;

    uint8_t tx_buf[SOMEIP_TX_BUFFER_SIZE];
    someip_header_t *hdr = (someip_header_t *)tx_buf;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(SOMEIP_NOTIFY_PERIOD_MS));

        for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
        {
            if (!clients[i].active ||
                !clients[i].heartbeat_subscribed)
                continue;

            hdr->service_id   = 0x1234;
            hdr->method_id    = 0x0001;
            hdr->length       = SOMEIP_HEADER_PAYLOAD_OFFSET + 4;
            hdr->client_id    = 0;
            hdr->session_id   = 0;
            hdr->protocol_version  = 1;
            hdr->interface_version = 1;
            hdr->message_type = SOMEIP_MSG_NOTIFICATION;
            hdr->return_code  = SOMEIP_RET_OK;

            uint32_t alive = FreeRTOS_htonl(1);
            memcpy(tx_buf + sizeof(*hdr), &alive, sizeof(alive));

            someip_hton_header(hdr);

            FreeRTOS_send(
                clients[i].socket,
                tx_buf,
                sizeof(*hdr) + sizeof(alive),
                0
            );
        }
    }
}
