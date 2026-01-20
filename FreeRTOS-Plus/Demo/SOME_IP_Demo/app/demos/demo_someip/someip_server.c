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
#define SOMEIP_LISTEN_BACKLOG  2
#define SOMEIP_RECV_TIMEOUT_MS 5000

/* =========================================================
 * Forward declarations
 * ========================================================= */
static void someip_server_task(void *arg);

/* =========================================================
 * Deterministic TCP receive helper
 * ========================================================= */
static BaseType_t recv_exact(Socket_t sock, uint8_t *buf, size_t len)
{
    size_t received = 0;

    while (received < len)
    {
        int r = FreeRTOS_recv(
            sock,
            buf + received,
            len - received,
            0
        );

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
 * TCP + SOME/IP server task
 * ========================================================= */
static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t listen_sock, client_sock;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    uint8_t rx_buf[SOMEIP_RX_BUFFER_SIZE];
    uint8_t tx_buf[SOMEIP_TX_BUFFER_SIZE];

    FreeRTOS_printf(("SOME/IP: Creating socket\r\n"));

    listen_sock = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    configASSERT(listen_sock != FREERTOS_INVALID_SOCKET);

    addr.sin_port = FreeRTOS_htons(SOMEIP_SERVER_PORT);
    FreeRTOS_bind(listen_sock, &addr, sizeof(addr));

    FreeRTOS_listen(listen_sock, SOMEIP_LISTEN_BACKLOG);

    FreeRTOS_printf(("SOME/IP: Listening on port %u\r\n", SOMEIP_SERVER_PORT));

    for (;;)
    {
        client_sock = FreeRTOS_accept(listen_sock, &addr, &addrlen);
        if (client_sock == FREERTOS_INVALID_SOCKET)
            continue;

        FreeRTOS_printf(("SOME/IP: Client connected\r\n"));

        /* ---- Apply receive timeout (dead client protection) ---- */
        TickType_t recv_timeout = pdMS_TO_TICKS(SOMEIP_RECV_TIMEOUT_MS);
        FreeRTOS_setsockopt(
            client_sock,
            0,
            FREERTOS_SO_RCVTIMEO,
            &recv_timeout,
            sizeof(recv_timeout)
        );

        for (;;)
        {
            someip_header_t hdr;

            /* ---- Receive header (deterministic) ---- */
            if (recv_exact(
                    client_sock,
                    rx_buf,
                    sizeof(someip_header_t)) != pdPASS)
            {
                break;
            }

            memcpy(&hdr, rx_buf, sizeof(hdr));
            someip_ntoh_header(&hdr);

            /* ---- Validate header ---- */
            if (someip_validate_header(&hdr) != pdPASS)
            {
                hdr.message_type = SOMEIP_MSG_ERROR;
                hdr.return_code  = SOMEIP_RET_E_MALFORMED_MESSAGE;
                hdr.length       = SOMEIP_HEADER_PAYLOAD_OFFSET;
                goto send_response;
            }

            /* ---- Payload length checks ---- */
            if (hdr.length < SOMEIP_HEADER_PAYLOAD_OFFSET)
            {
                hdr.message_type = SOMEIP_MSG_ERROR;
                hdr.return_code  = SOMEIP_RET_E_MALFORMED_MESSAGE;
                hdr.length       = SOMEIP_HEADER_PAYLOAD_OFFSET;
                goto send_response;
            }

            uint32_t payload_len =
                hdr.length - SOMEIP_HEADER_PAYLOAD_OFFSET;

            if (payload_len >
                (SOMEIP_RX_BUFFER_SIZE - sizeof(someip_header_t)))
            {
                hdr.message_type = SOMEIP_MSG_ERROR;
                hdr.return_code  = SOMEIP_RET_E_MALFORMED_MESSAGE;
                hdr.length       = SOMEIP_HEADER_PAYLOAD_OFFSET;
                goto send_response;
            }

            /* ---- Receive payload (deterministic) ---- */
            if (payload_len > 0)
            {
                if (recv_exact(
                        client_sock,
                        rx_buf + sizeof(hdr),
                        payload_len) != pdPASS)
                {
                    break;
                }
            }

            /* ---- Dispatch ---- */
            someip_service_handler_t handler =
                someip_find_service(hdr.service_id);

            uint32_t resp_len = 0;
            someip_return_code_t ret = SOMEIP_RET_OK;

            if (handler == NULL ||
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

        send_response:
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

        FreeRTOS_shutdown(client_sock, FREERTOS_SHUT_RDWR);
        FreeRTOS_closesocket(client_sock);
    }
}
