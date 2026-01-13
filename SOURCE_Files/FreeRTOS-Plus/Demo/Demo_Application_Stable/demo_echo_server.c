#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"
#include "demo_echo_server.h"

#define ECHO_SERVER_PORT   5001
#define ECHO_STACK_SIZE   ( configMINIMAL_STACK_SIZE * 2 )
#define ECHO_PRIORITY     ( tskIDLE_PRIORITY + 1 )

static void prvEchoServerTask( void * pvParameters )
{
    Socket_t xListenSocket, xClientSocket;
    struct freertos_sockaddr xBindAddress;
    char rxBuffer[128];
    BaseType_t xRx;

    FreeRTOS_printf( ( "ECHO SERVER: Starting\r\n" ) );

    xListenSocket = FreeRTOS_socket(
        FREERTOS_AF_INET,
        FREERTOS_SOCK_STREAM,
        FREERTOS_IPPROTO_TCP
    );

    configASSERT( xListenSocket != FREERTOS_INVALID_SOCKET );

    xBindAddress.sin_port = FreeRTOS_htons( ECHO_SERVER_PORT );

    FreeRTOS_bind( xListenSocket, &xBindAddress, sizeof( xBindAddress ) );
    FreeRTOS_listen( xListenSocket, 2 );

    for( ;; )
    {
        FreeRTOS_printf( ( "ECHO SERVER: Waiting for client...\r\n" ) );

        xClientSocket = FreeRTOS_accept( xListenSocket, NULL, NULL );
        if( xClientSocket == FREERTOS_INVALID_SOCKET )
            continue;

        FreeRTOS_printf( ( "ECHO SERVER: Client connected\r\n" ) );

        for( ;; )
        {
            xRx = FreeRTOS_recv(
                xClientSocket,
                rxBuffer,
                sizeof( rxBuffer ),
                0
            );

            if( xRx <= 0 )
                break;

            FreeRTOS_send(
                xClientSocket,
                rxBuffer,
                xRx,
                0
            );
        }

        FreeRTOS_printf( ( "ECHO SERVER: Client disconnected\r\n" ) );
        FreeRTOS_closesocket( xClientSocket );
    }
}

void vStartEchoServer( void )
{
    xTaskCreate(
        prvEchoServerTask,
        "EchoServer",
        ECHO_STACK_SIZE,
        NULL,
        ECHO_PRIORITY,
        NULL
    );
}
