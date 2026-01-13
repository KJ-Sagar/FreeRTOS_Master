#include "someip_server.h"
#include "someip_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"

#define SOMEIP_PORT  30509

static void someip_server_task(void *arg)
{
    Socket_t server, client;
    struct freertos_sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    addr.sin_port = FreeRTOS_htons(SOMEIP_PORT);

    server = FreeRTOS_socket(FREERTOS_AF_INET,
                             FREERTOS_SOCK_STREAM,
                             FREERTOS_IPPROTO_TCP);

    FreeRTOS_bind(server, &addr, sizeof(addr));
    FreeRTOS_listen(server, 1);

    client = FreeRTOS_accept(server, &addr, &addrlen);

    for (;;)
    {
        someip_header_t req;
        FreeRTOS_recv(client, &req, sizeof(req), FREERTOS_MSG_WAITALL);
        someip_ntoh(&req);

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
    xTaskCreate(someip_server_task,
                "SOMEIP_SERVER",
                configMINIMAL_STACK_SIZE * 4,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
}
