/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc.
 * All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */

/* ================= Standard includes ================= */
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* ================= FreeRTOS includes ================= */
#include "FreeRTOS.h"
#include "task.h"

/* ================= FreeRTOS+TCP includes ================= */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* ================= Platform / CMSIS ================= */
#include "CMSIS/CMSDK_CM3.h"
#include "main_networking.h"

/* ================= Application includes ================= */
#include "app_main.h"
#include "app/demos/demo_someip/heartbeat_service.h"


/* ================= Host name ================= */
#define mainHOST_NAME        "RTOSDemo"
#define mainDEVICE_NICK_NAME "qemu_demo"

/* ================= Forward declarations ================= */
static void prvSRand( UBaseType_t ulSeed );
static void prvMiscInitialisation( void );

/* ================= Network configuration ================= */
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

/* ================= RNG state ================= */
static UBaseType_t ulNextRand;

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
static NetworkInterface_t xInterfaces[ 1 ];
static NetworkEndPoint_t  xEndPoints[ 1 ];
#endif

/* =========================================================
 * main_tcp_network_init
 * ========================================================= */
void main_tcp_network_init( void )
{
    BaseType_t xReturn;

    prvMiscInitialisation();

    FreeRTOS_debug_printf( ( "FreeRTOS_IPInit\r\n" ) );

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

    for( ;; );
}

/* =========================================================
 * Network event hook
 * ========================================================= */
static BaseType_t xTasksAlreadyCreated = pdFALSE;

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

        /* ---- Application init (no sockets here) ---- */
        vApplicationStart();
        xTasksAlreadyCreated = pdTRUE;
    }
}

/* =========================================================
 * DNS hooks (unused)
 * ========================================================= */
#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )
BaseType_t xApplicationDNSQueryHook_Multi(
    struct xNetworkEndPoint * pxEndPoint,
    const char * pcName )
{
    ( void ) pxEndPoint;
    ( void ) pcName;
    return pdFAIL;
}
#endif

/* =========================================================
 * Random helpers
 * ========================================================= */
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

/* =========================================================
 * TCP stack callbacks
 * ========================================================= */
uint32_t ulApplicationGetNextSequenceNumber( uint32_t a,
                                             uint16_t b,
                                             uint32_t c,
                                             uint16_t d )
{
    ( void ) a; ( void ) b; ( void ) c; ( void ) d;
    return uxRand();
}

BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber )
{
    *pulNumber = uxRand();
    return pdTRUE;
}
