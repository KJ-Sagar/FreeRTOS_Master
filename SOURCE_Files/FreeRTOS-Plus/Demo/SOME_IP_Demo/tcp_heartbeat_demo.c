#include <stdio.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/*-----------------------------------------------------------*/
/* Configuration */
#define HEARTBEAT_SERVER_IP    "10.0.0.1"
#define HEARTBEAT_SERVER_PORT  5001

#define HEARTBEAT_TASK_STACK   ( configMINIMAL_STACK_SIZE * 2 )
#define HEARTBEAT_TASK_PRIO    ( tskIDLE_PRIORITY + 1 )
#define HEARTBEAT_PERIOD_MS   1000
/*-----------------------------------------------------------*/

static void prvTCPHeartbeatTask( void * pvParameters );

/*-----------------------------------------------------------*/

void vStartTCPHeartbeatDemo( void )
{
    xTaskCreate( prvTCPHeartbeatTask,
                 "TCP-HB",
                 HEARTBEAT_TASK_STACK,
                 NULL,
                 HEARTBEAT_TASK_PRIO,
                 NULL );
}

/*-----------------------------------------------------------*/

static void prvTCPHeartbeatTask( void * pvParameters )
{
    Socket_t xSocket;
    struct freertos_sockaddr xServerAddress;
    BaseType_t xResult;
    uint32_t ulCounter = 0;
    char cTxBuffer[ 64 ];
    TickType_t xDelay = pdMS_TO_TICKS( HEARTBEAT_PERIOD_MS );

    ( void ) pvParameters;

    for( ;; )
    {
        /* Create socket */
        xSocket = FreeRTOS_socket( FREERTOS_AF_INET,
                                   FREERTOS_SOCK_STREAM,
                                   FREERTOS_IPPROTO_TCP );

        if( xSocket == FREERTOS_INVALID_SOCKET )
        {
            vTaskDelay( pdMS_TO_TICKS( 2000 ) );
            continue;
        }

        /* Set send timeout */
        {
            TickType_t xSendTimeout = pdMS_TO_TICKS( 2000 );
            FreeRTOS_setsockopt( xSocket,
                                 0,
                                 FREERTOS_SO_SNDTIMEO,
                                 &xSendTimeout,
                                 sizeof( xSendTimeout ) );
        }

        /* Server address */
        memset( &xServerAddress, 0, sizeof( xServerAddress ) );

        xServerAddress.sin_family = FREERTOS_AF_INET;   
        xServerAddress.sin_port   = FreeRTOS_htons( HEARTBEAT_SERVER_PORT );
        xServerAddress.sin_address.ulIP_IPv4 =
    
        FreeRTOS_inet_addr( HEARTBEAT_SERVER_IP );
/*
        xServerAddress.sin_address.ulIP_IPv4 = FreeRTOS_inet_addr( HEARTBEAT_SERVER_IP );
        xServerAddress.sin_port = FreeRTOS_htons( HEARTBEAT_SERVER_PORT );
*/
        FreeRTOS_printf( ( "HB: Connecting to %s:%d\r\n",
                            HEARTBEAT_SERVER_IP,
                            HEARTBEAT_SERVER_PORT ) );

        xResult = FreeRTOS_connect( xSocket,
                                    &xServerAddress,
                                    sizeof( xServerAddress ) );

        if( xResult != 0 )
        {
            FreeRTOS_printf( ( "HB: Connect failed\r\n" ) );
            FreeRTOS_closesocket( xSocket );
            vTaskDelay( pdMS_TO_TICKS( 3000 ) );
            continue;
        }

        FreeRTOS_printf( ( "HB: Connected\r\n" ) );


        /* Heartbeat loop */
        for( ;; )
        {
            int len = snprintf( cTxBuffer,
                                sizeof( cTxBuffer ),
                                "HEARTBEAT %lu\r\n",
                                ( unsigned long ) ulCounter++ );

            xResult = FreeRTOS_send( xSocket,
                                     cTxBuffer,
                                     len,
                                     0 );

            if( xResult < 0 )
            {
                FreeRTOS_printf( ( "HB: Send failed, reconnecting\r\n" ) );
                break;
            }

            vTaskDelay( xDelay );
        }

        FreeRTOS_closesocket( xSocket );
        vTaskDelay( pdMS_TO_TICKS( 2000 ) );
    }
}
