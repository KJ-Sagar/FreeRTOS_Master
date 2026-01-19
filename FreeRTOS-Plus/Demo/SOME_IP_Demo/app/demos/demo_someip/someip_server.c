#include "someip_server.h"
#include "someip_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "stdio.h"

/* -------------------------------------------------
 * Simple PRNG (deterministic, no libc rand)
 * ------------------------------------------------- */
static uint32_t rng_state = 0xA5A5A5A5;

static uint32_t rand32(void)
{
    rng_state = (1103515245UL * rng_state + 12345UL);
    return rng_state;
}

/* -------------------------------------------------
 * Forward declaration
 * ------------------------------------------------- */
static void someip_server_task(void *arg);

/* -------------------------------------------------
 * Public entry point
 * ------------------------------------------------- */
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

/* -------------------------------------------------
 * SOME/IP Server Task
 * ------------------------------------------------- */
static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t server_socket, client_socket;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    uint8_t rx_buf[SOMEIP_RX_BUFFER_SIZE];
    uint8_t tx_buf[SOMEIP_TX_BUFFER_SIZE];

    /* ---- Create listening socket ---- */
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

    /* ---- Accept loop ---- */
    for (;;)
    {
        client_socket = FreeRTOS_accept(server_socket, &addr, &addrlen);

        if (client_socket == FREERTOS_INVALID_SOCKET)
        {
            continue;
        }

        FreeRTOS_printf(("SOMEIP: Client connected\r\n"));

        /* -------------------------------------------------
         * Persistent session: serve requests until client
         * disconnects
         * ------------------------------------------------- */
        for (;;)
        {
            int rx_len = FreeRTOS_recv(
                client_socket,
                rx_buf,
                sizeof(rx_buf),
                0
            );

            /* Client closed connection or error */
            if (rx_len <= 0)
            {
                FreeRTOS_printf(("SOMEIP: Client disconnected\r\n"));
                break;
            }

            /* ---- Basic sanity check ---- */
            if (rx_len < (int)sizeof(someip_header_t))
            {
                FreeRTOS_printf(("SOMEIP: Short packet ignored\r\n"));
                continue;
            }

            someip_header_t *hdr = (someip_header_t *)rx_buf;
            someip_ntoh_header(hdr);

            uint8_t *payload = tx_buf + sizeof(someip_header_t);
            uint32_t payload_len = 0;

            /* ---- Default response ---- */
            hdr->message_type = SOMEIP_MSG_RESPONSE;
            hdr->return_code  = SOMEIP_RET_OK;

            /* ---- Validate message type ---- */
            if (hdr->message_type != SOMEIP_MSG_REQUEST)
            {
                hdr->message_type = SOMEIP_MSG_ERROR;
                hdr->return_code  = SOMEIP_RET_E_MALFORMED_MESSAGE;
            }
            else
            {
                /* ---- Service dispatch ---- */
                switch (hdr->service_id)
                {
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
                        else
                        {
                            hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                        }
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
                        {
                            hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                        }
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
                        {
                            hdr->return_code = SOMEIP_RET_E_UNKNOWN_METHOD;
                        }
                        break;

                    default:
                        hdr->return_code = SOMEIP_RET_E_UNKNOWN_SERVICE;
                        break;
                }
            }

            /* ---- Finalize header ---- */
            hdr->length = payload_len;
            someip_hton_header(hdr);

            memcpy(tx_buf, hdr, sizeof(someip_header_t));

            /* ---- Send response ---- */
            FreeRTOS_send(
                client_socket,
                tx_buf,
                sizeof(someip_header_t) + payload_len,
                0
            );
        }

        FreeRTOS_closesocket(client_socket);
    }
}
