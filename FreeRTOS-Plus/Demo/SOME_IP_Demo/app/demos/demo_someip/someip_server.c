#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#include "someip_server.h"
#include "someip_protocol.h"

#define SOMEIP_PORT 30509
#define SOMEIP_BACKLOG 1

/*-----------------------------------------------------------*/

static void someip_server_task(void *arg)
{
    (void) arg;

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
                        FREERTOS_IPPROTO_TCP );

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

    /* Accept client (blocking) */
    xClientSocket = FreeRTOS_accept(
                        xListenSocket,
                        &xClientAddress,
                        &xClientAddressLength );

    if (xClientSocket == FREERTOS_INVALID_SOCKET)
    {
        FreeRTOS_printf(("SOMEIP: Accept failed\r\n"));
        vTaskDelete(NULL);
    }

    FreeRTOS_printf(("SOMEIP: Client connected\r\n"));

    /*-------------------------------------------------------*/
    /* Request / Response loop                               */
    /*-------------------------------------------------------*/
    for (;;)
    {
        someip_header_t req;
        BaseType_t xRecv;

        /* Block until full SOME/IP header arrives */
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

        /* Prepare payload */
        int32_t temperature = FreeRTOS_htonl(250); /* 25.0 °C */

        /* Prepare response header */
        someip_header_t resp = req;
        resp.message_type = SOMEIP_MSG_RESPONSE;
        resp.length = sizeof(int32_t);
        someip_hton(&resp);

        /* Send response */
        FreeRTOS_send(xClientSocket, &resp, sizeof(resp), 0);
        FreeRTOS_send(xClientSocket, &temperature, sizeof(temperature), 0);
    }

    /* Cleanup */
    FreeRTOS_closesocket(xClientSocket);
    FreeRTOS_closesocket(xListenSocket);

    FreeRTOS_printf(("SOMEIP: Server stopped\r\n"));
    vTaskDelete(NULL);
}

/*-----------------------------------------------------------*/

void someip_server_start(void)
{
    FreeRTOS_printf(("SOMEIP: Server start requested\r\n"));

    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL );
}
/*-----------------------------------------------------------*/