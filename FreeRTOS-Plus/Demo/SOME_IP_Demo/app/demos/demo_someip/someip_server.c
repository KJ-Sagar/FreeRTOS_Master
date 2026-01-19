#include "someip_server.h"
#include "someip_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "string.h"
#include "stdio.h"

/* =================================================
 * Configuration
 * ================================================= */
#define SOMEIP_MAX_CLIENTS 4

typedef struct
{
    Socket_t socket;
    BaseType_t active;
    BaseType_t temp_subscribed;
    SemaphoreHandle_t tx_mutex;
} someip_client_ctx_t;

static someip_client_ctx_t clients[SOMEIP_MAX_CLIENTS];

/* =================================================
 * Client context management
 * ================================================= */
static someip_client_ctx_t *alloc_client(Socket_t sock)
{
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (!clients[i].active)
        {
            clients[i].active = pdTRUE;
            clients[i].socket = sock;
            clients[i].temp_subscribed = pdFALSE;
            clients[i].tx_mutex = xSemaphoreCreateMutex();
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
            clients[i].temp_subscribed = pdFALSE;

            if (clients[i].tx_mutex)
            {
                vSemaphoreDelete(clients[i].tx_mutex);
                clients[i].tx_mutex = NULL;
            }

            clients[i].socket = FREERTOS_INVALID_SOCKET;
        }
    }
}

/* =================================================
 * Simple deterministic PRNG
 * ================================================= */
static uint32_t rng_state = 0xA5A5A5A5;

static uint32_t rand32(void)
{
    rng_state = (1103515245UL * rng_state + 12345UL);
    return rng_state;
}

/* =================================================
 * TCP reassembly helper (blocking)
 * ================================================= */
static int recv_exact(Socket_t sock, uint8_t *buf, size_t len)
{
    size_t total = 0;

    while (total < len)
    {
        int r = FreeRTOS_recv(sock, buf + total, len - total, 0);
        if (r <= 0)
            return -1;
        total += r;
    }
    return 0;
}

/* =================================================
 * Forward declarations
 * ================================================= */
static void someip_server_task(void *arg);
static void someip_notification_task(void *arg);

/* =================================================
 * Public entry point
 * ================================================= */
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

    xTaskCreate(
        someip_notification_task,
        "SOMEIP_NOTIFY",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
}

/* =================================================
 * SOME/IP Server Task (request / response)
 * ================================================= */
static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t server_socket, client_socket;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    uint8_t rx_buf[SOMEIP_RX_BUFFER_SIZE];
    uint8_t tx_buf[SOMEIP_TX_BUFFER_SIZE];

    server_socket = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    configASSERT(server_socket != FREERTOS_INVALID_SOCKET);

    addr.sin_port = FreeRTOS_htons(SOMEIP_SERVER_PORT);
    FreeRTOS_bind(server_socket, &addr, sizeof(addr));
    FreeRTOS_listen(server_socket, 1);

    FreeRTOS_printf(("SOMEIP: Server listening on port %d\r\n",
                     SOMEIP_SERVER_PORT));

    for (;;)
    {
        client_socket = FreeRTOS_accept(server_socket, &addr, &addrlen);
        if (client_socket == FREERTOS_INVALID_SOCKET)
            continue;

        FreeRTOS_printf(("SOMEIP: Client connected\r\n"));

        someip_client_ctx_t *client = alloc_client(client_socket);
        if (!client)
        {
            FreeRTOS_printf(("SOMEIP: Max clients reached\r\n"));
            FreeRTOS_closesocket(client_socket);
            continue;
        }

        /* ---- IMPORTANT: make recv() blocking ---- */
        TickType_t recv_timeout = portMAX_DELAY;
        FreeRTOS_setsockopt(
            client_socket,
            0,
            FREERTOS_SO_RCVTIMEO,
            &recv_timeout,
            sizeof(recv_timeout)
        );

        for (;;)
        {
            /* ---- Receive header ---- */
            if (recv_exact(client_socket,
                           rx_buf,
                           sizeof(someip_header_t)) < 0)
                break;

            someip_header_t *hdr = (someip_header_t *)rx_buf;
            someip_ntoh_header(hdr);

            /* ---- Receive payload ---- */
            if (hdr->length > 0)
            {
                if (hdr->length >
                    (SOMEIP_RX_BUFFER_SIZE - sizeof(someip_header_t)))
                {
                    hdr->return_code = SOMEIP_RET_E_MALFORMED_MESSAGE;
                    goto send_response;
                }

                if (recv_exact(client_socket,
                               rx_buf + sizeof(someip_header_t),
                               hdr->length) < 0)
                    break;
            }

            uint8_t *payload = tx_buf + sizeof(someip_header_t);
            uint32_t payload_len = 0;

            uint8_t req_type = hdr->message_type;

            hdr->message_type = SOMEIP_MSG_RESPONSE;
            hdr->return_code  = SOMEIP_RET_OK;

            if (req_type != SOMEIP_MSG_REQUEST)
            {
                hdr->message_type = SOMEIP_MSG_ERROR;
                hdr->return_code  = SOMEIP_RET_E_MALFORMED_MESSAGE;
                goto send_response;
            }

            switch (hdr->service_id)
            {
                case SOMEIP_SERVICE_SD:
                {
                    if (hdr->method_id == SOMEIP_METHOD_SD_OFFER)
                    {
                        uint16_t services[] = {
                            FreeRTOS_htons(SOMEIP_SERVICE_SENSOR),
                            FreeRTOS_htons(SOMEIP_SERVICE_ENGINE),
                            FreeRTOS_htons(SOMEIP_SERVICE_SYSTEM)
                        };
                        memcpy(payload, services, sizeof(services));
                        payload_len = sizeof(services);
                    }
                    else
                        hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                    break;
                }

                case SOMEIP_SERVICE_SENSOR:
                    if (hdr->method_id == SOMEIP_METHOD_GET_TEMPERATURE)
                    {
                        int32_t temp = 150 + (rand32() % 250);
                        temp = FreeRTOS_htonl(temp);
                        memcpy(payload, &temp, sizeof(temp));
                        payload_len = sizeof(temp);
                    }
                    else if (hdr->method_id == SOMEIP_METHOD_GET_HUMIDITY)
                    {
                        payload[0] = 30 + (rand32() % 50);
                        payload_len = 1;
                    }
                    else if (hdr->method_id == SOMEIP_METHOD_SUBSCRIBE)
                        client->temp_subscribed = pdTRUE;
                    else if (hdr->method_id == SOMEIP_METHOD_UNSUBSCRIBE)
                        client->temp_subscribed = pdFALSE;
                    else
                        hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                    break;

                case SOMEIP_SERVICE_ENGINE:
                    if (hdr->method_id == SOMEIP_METHOD_GET_RPM)
                    {
                        uint16_t rpm =
                            FreeRTOS_htons(800 + (rand32() % 6200));
                        memcpy(payload, &rpm, sizeof(rpm));
                        payload_len = sizeof(rpm);
                    }
                    else if (hdr->method_id == SOMEIP_METHOD_GET_TORQUE)
                    {
                        uint16_t tq =
                            FreeRTOS_htons(100 + (rand32() % 400));
                        memcpy(payload, &tq, sizeof(tq));
                        payload_len = sizeof(tq);
                    }
                    else
                        hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                    break;

                case SOMEIP_SERVICE_SYSTEM:
                    if (hdr->method_id == SOMEIP_METHOD_GET_STATUS)
                    {
                        payload[0] = 1;
                        payload_len = 1;
                    }
                    else if (hdr->method_id == SOMEIP_METHOD_GET_UPTIME)
                    {
                        uint32_t up =
                            FreeRTOS_htonl(xTaskGetTickCount());
                        memcpy(payload, &up, sizeof(up));
                        payload_len = sizeof(up);
                    }
                    else
                        hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                    break;

                default:
                    hdr->return_code = SOMEIP_RET_E_UNKNOWN_SERVICE;
                    break;
            }

send_response:
            hdr->length = payload_len;
            hdr->message_type =
                (hdr->return_code == SOMEIP_RET_OK)
                    ? SOMEIP_MSG_RESPONSE
                    : SOMEIP_MSG_ERROR;

            someip_hton_header(hdr);
            memcpy(tx_buf, hdr, sizeof(*hdr));

            xSemaphoreTake(client->tx_mutex, portMAX_DELAY);
            FreeRTOS_send(
                client_socket,
                tx_buf,
                sizeof(*hdr) + payload_len,
                0
            );
            xSemaphoreGive(client->tx_mutex);
        }

        free_client(client_socket);
        FreeRTOS_closesocket(client_socket);
        FreeRTOS_printf(("SOMEIP: Client session closed\r\n"));
    }
}

/* =================================================
 * Notification Task (server → client push)
 * ================================================= */
static void someip_notification_task(void *arg)
{
    (void)arg;

    uint8_t tx_buf[SOMEIP_TX_BUFFER_SIZE];
    someip_header_t *hdr = (someip_header_t *)tx_buf;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
        {
            if (!clients[i].active ||
                !clients[i].temp_subscribed ||
                clients[i].socket == FREERTOS_INVALID_SOCKET)
                continue;

            hdr->service_id   = SOMEIP_SERVICE_SENSOR;
            hdr->method_id    = SOMEIP_METHOD_GET_TEMPERATURE;
            hdr->length       = sizeof(int32_t);
            hdr->message_type = SOMEIP_MSG_NOTIFICATION;
            hdr->return_code  = SOMEIP_RET_OK;
            hdr->client_id    = 0;
            hdr->session_id   = 0;

            int32_t temp = 150 + (rand32() % 250);
            temp = FreeRTOS_htonl(temp);
            memcpy(tx_buf + sizeof(*hdr), &temp, sizeof(temp));

            someip_hton_header(hdr);

            xSemaphoreTake(clients[i].tx_mutex, portMAX_DELAY);
            FreeRTOS_send(
                clients[i].socket,
                tx_buf,
                sizeof(*hdr) + sizeof(temp),
                0
            );
            xSemaphoreGive(clients[i].tx_mutex);
        }
    }
}
