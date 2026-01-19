#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#include "someip_server.h"
#include "someip_protocol.h"

#define SOMEIP_PORT     30509
#define SOMEIP_BACKLOG  1

/*-----------------------------------------------------------*/
//PRNG state
static uint32_t someip_rng_state = 0x12345678;

static uint32_t someip_rand(void)
{
    /* Linear Congruential Generator (LCG) */
    someip_rng_state = (1103515245 * someip_rng_state + 12345);
    return someip_rng_state;
}

static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t xListenSocket = FREERTOS_INVALID_SOCKET;
    Socket_t xClientSocket = FREERTOS_INVALID_SOCKET;

    struct freertos_sockaddr xBindAddress;
    struct freertos_sockaddr xClientAddress;
    socklen_t xClientAddressLength = sizeof(xClientAddress);

    /* Bind to ANY address on SOME/IP port */
    memset(&xBindAddress, 0, sizeof(xBindAddress));
    xBindAddress.sin_family = FREERTOS_AF_INET;
    xBindAddress.sin_port   = FreeRTOS_htons(SOMEIP_PORT);
    xBindAddress.sin_address.ulIP_IPv4 = FREERTOS_INADDR_ANY;

    /* Create listening socket */
    xListenSocket = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    configASSERT(xListenSocket != FREERTOS_INVALID_SOCKET);
    FreeRTOS_printf(("SOMEIP: Socket created\r\n"));

    /* Bind */
    configASSERT(
        FreeRTOS_bind(
            xListenSocket,
            &xBindAddress,
            sizeof(xBindAddress)) == 0 );

    FreeRTOS_printf(("SOMEIP: Bound to port %d\r\n", SOMEIP_PORT));

    /* Listen */
    configASSERT(
        FreeRTOS_listen(xListenSocket, SOMEIP_BACKLOG) == 0 );

    FreeRTOS_printf(("SOMEIP: Listening...\r\n"));

    /* ================= LOOP-BACK ACCEPT ================= */
    for (;;)
    {
        xClientSocket = FreeRTOS_accept(
            xListenSocket,
            &xClientAddress,
            &xClientAddressLength );

        if (xClientSocket == FREERTOS_INVALID_SOCKET)
        {
            FreeRTOS_printf(("SOMEIP: Accept failed\r\n"));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        FreeRTOS_printf(("SOMEIP: Client connected\r\n"));

        /* Make recv() blocking */
        {
            TickType_t xRecvTimeout = portMAX_DELAY;
            FreeRTOS_setsockopt(
                xClientSocket,
                0,
                FREERTOS_SO_RCVTIMEO,
                &xRecvTimeout,
                sizeof(xRecvTimeout)
            );
        }

        /* -------- Handle client -------- */
        for (;;)
        {
            someip_header_t req;
            BaseType_t xRecv;

            xRecv = FreeRTOS_recv(
                xClientSocket,
                &req,
                sizeof(req),
                0 );

            if (xRecv <= 0)
            {
                FreeRTOS_printf(("SOMEIP: Client disconnected\r\n"));
                break;
            }

            someip_ntoh(&req);

            FreeRTOS_printf((
                "SOMEIP: Req svc=0x%04x method=0x%04x\r\n",
                req.service_id,
                req.method_id));

           /* /* Prepare payload */
            // int32_t temperature = FreeRTOS_htonl(250); /* 25.0 °C */

            // /* Prepare response */
            // someip_header_t resp = req;
            // resp.message_type = SOMEIP_MSG_RESPONSE;
            // resp.length = sizeof(int32_t);
            // someip_hton(&resp);

            // FreeRTOS_send(xClientSocket, &resp, sizeof(resp), 0);
            // FreeRTOS_send(xClientSocket, &temperature, sizeof(temperature), 0);
someip_header_t resp = req;
resp.message_type = SOMEIP_MSG_RESPONSE;
resp.return_code  = SOMEIP_E_OK;

uint8_t payload[8];
uint32_t payload_len = 0;

switch (req.method_id)
{
   case SOMEIP_METHOD_GET_TEMPERATURE:
{
    /* Generate temperature: 15.0°C – 40.0°C (scaled ×10) */
    int32_t temp_x10 = 150 + (someip_rand() % 250); // 150 → 400
    int32_t temp_net = FreeRTOS_htonl(temp_x10);

    memcpy(payload, &temp_net, sizeof(temp_net));
    payload_len = sizeof(temp_net);

    FreeRTOS_printf((
        "SOMEIP: Temperature = %ld.%ld C\r\n",
        temp_x10 / 10,
        temp_x10 % 10));

    break;
}


case SOMEIP_METHOD_GET_RPM:
{
    /* Generate RPM: 800 – 7000 */
    uint16_t rpm = 800 + (someip_rand() % 6200);
    uint16_t rpm_net = FreeRTOS_htons(rpm);

    memcpy(payload, &rpm_net, sizeof(rpm_net));
    payload_len = sizeof(rpm_net);

    FreeRTOS_printf((
        "SOMEIP: RPM = %u\r\n", rpm));

    break;
}



case SOMEIP_METHOD_GET_STATUS:
{
    payload[0] = (someip_rand() & 0x1); // 0 or 1
    payload_len = 1;

    FreeRTOS_printf((
        "SOMEIP: Status = %s\r\n",
        payload[0] ? "OK" : "FAULT"));

    break;
}


default:
{
    resp.return_code = SOMEIP_E_UNKNOWN_METHOD;
    payload_len = 0;

    FreeRTOS_printf((
        "SOMEIP: Resp svc=0x%04x method=0x%04x -> UNKNOWN METHOD\r\n",
        req.service_id,
        req.method_id));
    break;
}

}

resp.length = payload_len;
someip_hton(&resp);

/* Send response */
FreeRTOS_send(xClientSocket, &resp, sizeof(resp), 0);

if (payload_len > 0)
{
    FreeRTOS_send(xClientSocket, payload, payload_len, 0);
}

            /* Optional: prevent tight loop if client spams */
            vTaskDelay(pdMS_TO_TICKS(100));


        }

        FreeRTOS_closesocket(xClientSocket);
        xClientSocket = FREERTOS_INVALID_SOCKET;
    }
}

/*-----------------------------------------------------------*/

void someip_server_start( void )
{
    FreeRTOS_printf( ( "SOMEIP: Server start requested\r\n" ) );

    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL );
}

/*-----------------------------------------------------------*/
/* EOF */