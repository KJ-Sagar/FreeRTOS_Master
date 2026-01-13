/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */

/*
 * This project is a cut down version of the project described on the following
 * link. Only the TCP echo clients / heartbeat demo are included in the build:
 * https://www.FreeRTOS.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
 */

/* Standard includes. */
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"

/* Demo application includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "CMSIS/CMSDK_CM3.h"
#include "main_networking.h"
#include "tcp_heartbeat_demo.h"
#include "app_main.h"

/*-----------------------------------------------------------*/
/* Define a name that will be used for LLMNR and NBNS searches. */
#define mainHOST_NAME        "RTOSDemo"
#define mainDEVICE_NICK_NAME "qemu_demo"

/*-----------------------------------------------------------*/
/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand( UBaseType_t ulSeed );

/*
 * Miscellaneous initialisation including seeding the random number generator.
 */
static void prvMiscInitialisation( void );

/*-----------------------------------------------------------*/
/* Network address configuration */
static const uint8_t ucIPAddress[ 4 ] =
{
    configIP_ADDR0,
    configIP_ADDR1,
    configIP_ADDR2,
    configIP_ADDR3
};

static const uint8_t ucNetMask[ 4 ] =
{
    configNET_MASK0,
    configNET_MASK1,
    configNET_MASK2,
    configNET_MASK3
};

static const uint8_t ucGatewayAddress[ 4 ] =
{
    configGATEWAY_ADDR0,
    configGATEWAY_ADDR1,
    configGATEWAY_ADDR2,
    configGATEWAY_ADDR3
};

static const uint8_t ucDNSServerAddress[ 4 ] =
{
    configDNS_SERVER_ADDR0,
    configDNS_SERVER_ADDR1,
    configDNS_SERVER_ADDR2,
    configDNS_SERVER_ADDR3
};

const uint8_t ucMACAddress[ 6 ] =
{
    configMAC_ADDR0,
    configMAC_ADDR1,
    configMAC_ADDR2,
    configMAC_ADDR3,
    configMAC_ADDR4,
    configMAC_ADDR5
};

/*-----------------------------------------------------------*/
/* Pseudo random number generator state */
static UBaseType_t ulNextRand;

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )

/* There is only one physical network interface. */
static NetworkInterface_t xInterfaces[ 1 ];

/* A single endpoint is sufficient for this demo. */
static NetworkEndPoint_t xEndPoints[ 1 ];

#endif
/*-----------------------------------------------------------*/

/*
 * main_tcp_network_init()
 *
 * Initialises the TCP/IP stack and starts the scheduler.
 * Tasks that depend on the network are created from the
 * network-up event hook.
 */
void main_tcp_network_init( void )
{
    BaseType_t xReturn;

    /* Perform miscellaneous initialisation. */
    prvMiscInitialisation();

    FreeRTOS_debug_printf( ( "FreeRTOS_IPInit\r\n" ) );

    /* Set Ethernet interrupt priority. */
    NVIC_SetPriority( ETHERNET_IRQn, configMAC_INTERRUPT_PRIORITY );

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )

    extern NetworkInterface_t *
    pxMPS2_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                    NetworkInterface_t * pxInterface );

    pxMPS2_FillInterfaceDescriptor( 0, &xInterfaces[ 0 ] );

    FreeRTOS_FillEndPoint( &xInterfaces[ 0 ],
                           &xEndPoints[ 0 ],
                           ucIPAddress,
                           ucNetMask,
                           ucGatewayAddress,
                           ucDNSServerAddress,
                           ucMACAddress );

#if ( ipconfigUSE_DHCP != 0 )
    xEndPoints[ 0 ].bits.bWantDHCP = pdTRUE;
#endif

    xReturn = FreeRTOS_IPInit_Multi();

#else

    xReturn = FreeRTOS_IPInit( ucIPAddress,
                               ucNetMask,
                               ucGatewayAddress,
                               ucDNSServerAddress,
                               ucMACAddress );
#endif

    configASSERT( xReturn == pdTRUE );

    FreeRTOS_debug_printf( ( "vTaskStartScheduler\r\n" ) );
    vTaskStartScheduler();

    /* Execution should never reach here. */
    for( ;; );
}
/*-----------------------------------------------------------*/

static BaseType_t xTasksAlreadyCreated = pdFALSE;

/*
 * Called by FreeRTOS+TCP when the network connects or disconnects.
 */
#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
void vApplicationIPNetworkEventHook_Multi( eIPCallbackEvent_t eNetworkEvent,
                                           NetworkEndPoint_t * pxEndPoint )
#else
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
#endif
{
    uint32_t ulIPAddress, ulNetMask, ulGateway, ulDNS;
    char cBuffer[ 16 ];

    ( void ) pxEndPoint;

    if( ( eNetworkEvent == eNetworkUp ) && ( xTasksAlreadyCreated == pdFALSE ) )
    {
#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
        FreeRTOS_GetEndPointConfiguration( &ulIPAddress,
                                           &ulNetMask,
                                           &ulGateway,
                                           &ulDNS,
                                           pxNetworkEndPoints );
#else
        FreeRTOS_GetAddressConfiguration( &ulIPAddress,
                                          &ulNetMask,
                                          &ulGateway,
                                          &ulDNS );
#endif

        FreeRTOS_printf( ( "\r\nNetwork configuration:\r\n" ) );

        FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
        FreeRTOS_printf( ( "IP Address: %s\r\n", cBuffer ) );

        /* Start demo application tasks here */
        vApplicationStart();
        xTasksAlreadyCreated = pdTRUE;

    }
}
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )

BaseType_t xApplicationDNSQueryHook_Multi(
    struct xNetworkEndPoint * pxEndPoint,
    const char * pcName
)
{
    /* This demo does not respond to DNS / LLMNR / NBNS name queries */
    ( void ) pxEndPoint;
    ( void ) pcName;

    return pdFAIL;
}

#endif

static UBaseType_t uxRand( void )
{
    ulNextRand = ( 0x015a4e35UL * ulNextRand ) + 1UL;
    return ( ulNextRand >> 16UL ) & 0x7fffUL;
}

static void prvSRand( UBaseType_t ulSeed )
{
    ulNextRand = ulSeed;
}

static void prvMiscInitialisation( void )
{
    time_t xTimeNow;
    time( &xTimeNow );
    prvSRand( ( UBaseType_t ) xTimeNow );
}
/*-----------------------------------------------------------*/

/*
 * Provide a random number for TCP initial sequence numbers.
 */
uint32_t ulApplicationGetNextSequenceNumber( uint32_t a,
                                             uint16_t b,
                                             uint32_t c,
                                             uint16_t d )
{
    ( void ) a; ( void ) b; ( void ) c; ( void ) d;
    return uxRand();
}

/*
 * Provide a random number to the TCP stack.
 */
BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber )
{
    *pulNumber = uxRand();
    return pdTRUE;
}
