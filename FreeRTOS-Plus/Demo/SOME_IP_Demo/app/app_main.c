#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "app_config.h"
#include "app_main.h"
#include "demo_echo.h"
#include "heartbeat_service.h"
#include "demo_someip/demo_someip.h"

/*-----------------------------------------------------------*/     

HeartbeatService_Init();
someip_server_start();

void vApplicationStart( void )
{
#if DEMO_SOMEIP
    FreeRTOS_printf( ( "APP: Starting SOME/IP demo\r\n" ) );
    demo_someip_start();

#elif DEMO_HEARTBEAT
    FreeRTOS_printf( ( "APP: Starting HEARTBEAT demo\r\n" ) );
    vStartTCPHeartbeatDemo();

#elif DEMO_ECHO
    FreeRTOS_printf( ( "APP: Starting ECHO demo\r\n" ) );
    vStartEchoDemo();

#else
    FreeRTOS_printf( ( "APP: No demo selected\r\n" ) );
#endif
}

/*-----------------------------------------------------------*/