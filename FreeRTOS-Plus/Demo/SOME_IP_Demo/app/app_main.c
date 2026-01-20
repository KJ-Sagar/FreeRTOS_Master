#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

#include "app_config.h"
#include "app_main.h"
#include "app/demos/demo_someip/heartbeat_service.h"
#include "app/demos/demo_someip/sensor_service.h"
#include "app/demos/demo_someip/engine_service.h"
#include "demo_someip/demo_someip.h"
#include "demo_someip/someip_server.h"

#include "task.h"

/*-----------------------------------------------------------*/     

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