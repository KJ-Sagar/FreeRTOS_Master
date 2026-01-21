/*
 * TCP Echo Server Demo for FreeRTOS+TCP (QEMU MPS2)
 *
 * Acts as a TCP server running inside QEMU.
 * Linux host connects using netcat and exchanges data.
 */

#include <string.h>     /* memset */
#include <stddef.h>     /* NULL */

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOSIPConfig.h"

#include "demo_echo_server.h"

/*-----------------------------------------------------------*/
/* Configuration */

#define ECHO_SERVER_PORT        5001
#define ECHO_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 2 )
#define ECHO_TASK_PRIORITY      ( tskIDLE_PRIORITY + 1 )
#define ECHO_RX_TIMEOUT_MS      5000

/*-----------------------------------------------------------*/
/* Echo server task */

static void prvEchoServerTask( void * pvParameters )
{
    Socket_t xListenSocket = FREERTOS_INVALID_SOCKET;
    Socket_t xClientSocket = FREERTOS_INVALID_SOCKET;
    struct freertos_sockaddr xBindAddress;
    char rxBuffer[ 128 ];
    BaseType_t xRx;
    //TickType_t xTimeout = pdMS_TO_TICKS( ECHO_RX_TIMEOUT_MS );
    TickType_t xReceieveTimeout = portMAX_DELAY;
    ( void ) pvParameters;

    FreeRTOS_printf( ( "\r\n[ECHO SERVER] Starting on port %d\r\n",
                       ECHO_SERVER_PORT ) );

    /* Create TCP socket */
    xListenSocket = FreeRTOS_socket(
                        FREERTOS_AF_INET,
                        FREERTOS_SOCK_STREAM,
                        FREERTOS_IPPROTO_TCP );

    configASSERT( xListenSocket != FREERTOS_INVALID_SOCKET );

    /* Bind socket to port */
    memset( &xBindAddress, 0, sizeof( xBindAddress ) );
    xBindAddress.sin_port = FreeRTOS_htons( ECHO_SERVER_PORT );

    FreeRTOS_bind( xListenSocket,
                   &xBindAddress,
                   sizeof( xBindAddress ) );

    /* Start listening */
    FreeRTOS_listen( xListenSocket, 2 );

    for( ;; )
    {
        FreeRTOS_printf( ( "[ECHO SERVER] Waiting for client...\r\n" ) );

        /* Block until client connects */
        xClientSocket = FreeRTOS_accept( xListenSocket, NULL, NULL );

        if( xClientSocket == FREERTOS_INVALID_SOCKET )
        {
            continue;
        }

        /* Set receive timeout */
        FreeRTOS_setsockopt(
            xClientSocket,
            0,
            FREERTOS_SO_RCVTIMEO,
            &xReceieveTimeout,
            sizeof( xReceieveTimeout ) );

        FreeRTOS_printf( ( "[ECHO SERVER] Client connected\r\n" ) );

        for( ;; )
        {
            xRx = FreeRTOS_recv(
                      xClientSocket,
                      rxBuffer,
                      sizeof( rxBuffer ) - 1,
                      0 );

            if( xRx <= 0 )
            {
                break;
            }

            rxBuffer[ xRx ] = '\0';

            FreeRTOS_printf( ( "[ECHO SERVER RX] %s\r\n", rxBuffer ) );

            /* Echo back */
            FreeRTOS_send(
                xClientSocket,
                rxBuffer,
                xRx,
                0 );
        }

        FreeRTOS_printf( ( "[ECHO SERVER] Client disconnected\r\n" ) );
        FreeRTOS_closesocket( xClientSocket );
    }
}

/*-----------------------------------------------------------*/
/* Public API */

void vStartEchoServer( void )
{
    FreeRTOS_printf( ( "[ECHO SERVER] Creating server task\r\n" ) );

    xTaskCreate(
        prvEchoServerTask,
        "EchoServer",
        ECHO_STACK_SIZE,
        NULL,
        ECHO_TASK_PRIORITY,
        NULL );
}
