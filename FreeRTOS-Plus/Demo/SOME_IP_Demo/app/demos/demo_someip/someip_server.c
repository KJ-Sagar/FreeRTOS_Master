#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#include "someip_server.h"
#include "someip_protocol.h"


#define SOMEIP_PORT 30509

static void someip_server_task(void *arg)
{
    (void)arg;

    Socket_t server, client;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = FREERTOS_AF_INET;
    addr.sin_port   = FreeRTOS_FreeRTOS_htons(SOMEIP_PORT);
    addr.sin_address.ulIP_IPv4 = FreeRTOS_GetIPAddress();


    server = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    // Ensure the socket was created successfully.
    configASSERT(server != FREERTOS_INVALID_SOCKET);
    FreeRTOS_printf(("SOMEIP: Socket created\r\n"));

    // Bind the socket to the port.
    FreeRTOS_bind(server, &addr, sizeof(addr));
    FreeRTOS_printf(("SOMEIP: Bound to port %d\r\n", SOMEIP_PORT));
    
    // Listen for incoming connections.
    FreeRTOS_listen(server, 1);
    FreeRTOS_printf(("SOMEIP: Listening...\r\n"));
    
    // Accept a client connection.
    client = FreeRTOS_accept(server, &addr, &addrlen);
    FreeRTOS_printf(("SOMEIP: Client connected\r\n"));


    for (;;)
    {
        someip_header_t req;
        FreeRTOS_recv(client, &req, sizeof(req), 0);
        someip_FreeRTOS_ntohs(&req);
        FreeRTOS_printf(("SOMEIP: Request received\r\n"));

        int32_t temperature = 250; // 25.0 C
        temperature = FreeRTOS_htonl(temperature);

        someip_header_t resp = req;
        resp.message_type = SOMEIP_MSG_RESPONSE;
        resp.length = sizeof(int32_t);
        someip_hton(&resp);

        FreeRTOS_send(client, &resp, sizeof(resp), 0);
        FreeRTOS_send(client, &temperature, sizeof(temperature), 0);
    }
}

void someip_server_start(void)
{
    FreeRTOS_printf( ( "SOMEIP: Server start requested\r\n" ) );
    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
}
